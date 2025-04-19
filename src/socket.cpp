#include "socket.hpp"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "config.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/unity.hpp"

using namespace websocketpp;

static std::unique_ptr<server<config::asio>> socketServer;
static std::shared_mutex connectionsMutex;
static std::set<connection_hdl, std::owner_less<connection_hdl>> connections;
static bool threadRunning = false;

static void OpenHandler(connection_hdl connection) {
    logger.info("connected: {}", connection.lock().get());
    std::unique_lock lock(connectionsMutex);
    connections.emplace(connection);
    MetaCore::Engine::ScheduleMainThread([]() {
        Manager::SendSettings();
        Manager::RestartCapture();
    });
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
    MetaCore::Engine::ScheduleMainThread([packet = std::move(packet), source = connection.lock().get()]() { Manager::HandleMessage(packet, source); });
}

static bool StartListen() {
    try {
        int port = std::stoi(getConfig().Port.GetValue());
        socketServer->listen(lib::asio::ip::tcp::v4(), port);

        socketServer->start_accept();

        std::thread([]() {
            threadRunning = true;
            socketServer->run();
            threadRunning = false;
        }).detach();

        lib::asio::error_code ec;
        auto endpoint = socketServer->get_local_endpoint(ec);

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
                logger.info("v4 {}:{}", endpoint.address().to_v4().to_string(ec), endpoint.port());

            if (endpoint.address().is_v6())
                logger.info(
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

void Socket::Refresh(std::function<void(bool)> done) {
    try {
        if (threadRunning) {
            socketServer->stop_listening();

            std::unique_lock lock(connectionsMutex);
            for (auto& connection : connections)
                socketServer->close(connection, close::status::going_away, "configuration change");
            connections.clear();
        }

        if (done)
            MetaCore::Engine::ScheduleMainThread([]() { return !threadRunning; }, [done]() { done(StartListen()); });
        else
            MetaCore::Engine::ScheduleMainThread([]() { return !threadRunning; }, StartListen);
    } catch (std::exception const& exc) {
        logger.error("socket closing failed: {}", exc.what());
    }
}

void Socket::Init() {
    if (socketServer)
        return;

    logger.info("creating socket");
    try {
        socketServer = std::make_unique<server<config::asio>>();
        socketServer->set_access_channels(log::alevel::none);
        socketServer->set_error_channels(log::elevel::none);

        socketServer->init_asio();
        socketServer->set_reuse_addr(true);

        socketServer->set_open_handler(OpenHandler);
        socketServer->set_close_handler(CloseHandler);
        socketServer->set_message_handler(MessageHandler);

        StartListen();
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
            socketServer->send(hdl, string, frame::opcode::value::BINARY);
        } catch (std::exception const& e) {
            logger.error("send failed: {}", e.what());
        }
    }
}
