#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <thread>
#include <mutex>

std::unordered_map<std::string, std::pair<int, int>> playerPositions;
std::mutex playerPositionsMutex;

std::string player_position_to_json(const std::string& id, int x, int y) {
  nlohmann::json json;
  json["id"] = id;
  json["x"] = x;
  json["y"] = y;
  return json.dump();
}

void process_data(const std::string &data, boost::asio::ip::tcp::socket& socket) {
  try {
    std::cout << "Received data: " << data << std::endl;
    auto json = nlohmann::json::parse(data);

    if (json.find("id") == json.end()) {
      std::cout << "ID key not found in received data" << std::endl;
      return;
    }

    if (json.find("x") != json.end() && json.find("y") != json.end() && json["x"].is_number() && json["y"].is_number()) {
      int x = json["x"];
      int y = json["y"];

      {
        std::lock_guard<std::mutex> lock(playerPositionsMutex);
        playerPositions[json["id"]] = std::make_pair(x, y);
      }

      std::string response = player_position_to_json(json["id"], x, y);

      boost::system::error_code error;
      boost::asio::write(socket, boost::asio::buffer(response), error);
      if (error) {
          std::cerr << "Failed to write to socket. Error: " << error.message() << std::endl;
          return;
      }

      std::cout << "Updated position for " << json["id"] << " to (" << x << ", " << y << ")\n";
    } else {
      std::cout << "X and Y keys not found or incorrect type in received data" << std::endl;
      return;
    }
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << '\n';
  }
}

void client_handler(boost::asio::ip::tcp::socket socket) {
  std::array<char, 128> buf;
  boost::system::error_code error;

  for (;;) {
    size_t len = socket.read_some(boost::asio::buffer(buf), error);
    if (error == boost::asio::error::eof) break;
    else if (error) throw boost::system::system_error(error);

    std::string data(buf.begin(), buf.begin() + len);
    process_data(data, socket);
  }
}

int main() {
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8080));

  for (;;) {
    boost::asio::ip::tcp::socket socket(io_context);
    acceptor.accept(socket);
    std::thread(client_handler, std::move(socket)).detach();
  }
  return 0;
}
