#include "socket.hpp"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "config.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/unity.hpp"

using namespace websocketpp;

static bool initialized = false;
static server<config::asio> socketServer;
static std::shared_mutex connectionsMutex;
static std::set<connection_hdl, std::owner_less<connection_hdl>> connections;
static bool threadRunning = false;

static void OpenHandler(connection_hdl connection) {
    logger.info("connected: {}", connection.lock().get());
    std::unique_lock lock(connectionsMutex);
    connections.emplace(connection);
    MetaCore::Engine::ScheduleMainThread([]() { Manager::UpdateSettings(); });
}

static void CloseHandler(connection_hdl connection) {
    logger.info("disconnected: {}", connection.lock().get());
    std::unique_lock lock(connectionsMutex);
    connections.erase(connection);
    if (!connections.empty())
        return;
    MetaCore::Engine::ScheduleMainThread([]() {
        std::shared_lock lock(connectionsMutex);
        if (connections.empty())
            Manager::StopCapture();
    });
}

static void MessageHandler(connection_hdl connection, server<config::asio>::message_ptr message) {
    PacketWrapper packet;
    packet.ParseFromArray(message->get_payload().data(), message->get_payload().size());
    void* source = connection.lock().get();
    MetaCore::Engine::ScheduleMainThread([packet = std::move(packet), source]() { Manager::HandleMessage(packet, source); });
}

bool Socket::Start() {
    try {
        int port = std::stoi(getConfig().Port.GetValue());
        socketServer.listen(lib::asio::ip::tcp::v4(), port);

        socketServer.start_accept();

        std::thread([]() {
            threadRunning = true;
            socketServer.run();
            threadRunning = false;
        }).detach();

        lib::asio::error_code ec;
        auto endpoint = socketServer.get_local_endpoint(ec);

        if (ec.failed())
            logger.error("not listening: {}", ec.message());
        else {
            logger.info(
                "listening on {}:{} ipv4 {} ipv6 {}",
                endpoint.address().to_string(ec),
                endpoint.port(),
                endpoint.address().is_v4(),
                endpoint.address().is_v6()
            );

            if (endpoint.address().is_v4())
                logger.debug("v4 {}:{}", endpoint.address().to_v4().to_string(ec), endpoint.port());

            if (endpoint.address().is_v6())
                logger.debug(
                    "v6 {}:{} v4 compatible {}",
                    endpoint.address().to_v6().to_string(ec),
                    endpoint.port(),
                    endpoint.address().to_v6().is_v4_compatible()
                );
        }

        return true;
    } catch (std::exception const& exc) {
        logger.error("socket listen failed: {}", exc.what());
        return false;
    }
}

void Socket::Stop(std::function<void()> stopped) {
    try {
        if (threadRunning) {
            socketServer.stop_listening();

            std::unique_lock lock(connectionsMutex);
            for (auto& connection : connections)
                socketServer.close(connection, close::status::going_away, "configuration change");
            connections.clear();
        }
        if (stopped)
            MetaCore::Engine::ScheduleMainThread([]() { return !threadRunning; }, stopped);
    } catch (std::exception const& exc) {
        logger.error("socket closing failed: {}", exc.what());
    }
}

void Socket::Refresh(std::function<void(bool)> done) {
    if (done)
        Stop([done]() { done(Start()); });
    else
        Stop(Start);
}

void Socket::Init() {
    if (initialized)
        return;

    logger.info("initializing socket");
    try {
        socketServer.set_access_channels(log::alevel::none);
        socketServer.set_error_channels(log::elevel::none);

        socketServer.init_asio();
        socketServer.set_reuse_addr(true);

        socketServer.set_open_handler(OpenHandler);
        socketServer.set_close_handler(CloseHandler);
        socketServer.set_message_handler(MessageHandler);
    } catch (std::exception const& exc) {
        logger.error("socket init failed: {}", exc.what());
    }
}

void Socket::Send(PacketWrapper const& packet, void* exclude) {
    if (!packet.IsInitialized())
        return;
    auto string = packet.SerializeAsString();
    std::shared_lock lock(connectionsMutex);
    for (auto const& hdl : connections) {
        if (hdl.lock().get() == exclude)
            continue;
        try {
            socketServer.send(hdl, string, frame::opcode::value::BINARY);
        } catch (std::exception const& e) {
            logger.error("send failed: {}", e.what());
        }
    }
}
