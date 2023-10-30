#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>

std::unordered_map<std::string, std::pair<int, int>> playerPositions; 
std::mutex playerPositionsMutex;
std::atomic<int> playerCounter(0);

std::string player_position_to_json(const std::string& id, int x, int z) {
  nlohmann::json json;
  json["id"] = id;
  json["x"] = x;
  json["z"] = z;
  return json.dump();
}

void process_data(const std::string &data, boost::asio::ip::tcp::socket& socket) {
  try {
    nlohmann::json json = nlohmann::json::parse(data);
    std::cout << "Received data: " << data << std::endl;

    if (json.find("action") != json.end() && json["action"] == "disconnect") {
      std::string id = json["id"];
      {
        std::lock_guard<std::mutex> lock(playerPositionsMutex);
        playerPositions.erase(id);
      }
      std::cout << "Player " << id << " has disconnected\n";
      return;
    }

    if (json.find("id") == json.end()) {
      std::cout << "ID key not found in received data" << std::endl;
      return;
    }

    if (json.find("x") != json.end() && json.find("z") != json.end() && json["x"].is_number() && json["z"].is_number()) {
      int x = json["x"];
      int z = json["z"];
      {
        std::lock_guard<std::mutex> lock(playerPositionsMutex);
        playerPositions[json["id"]] = std::make_pair(x, z);
      }
      std::string response = player_position_to_json(json["id"], x, z);
      boost::system::error_code error;
      boost::asio::write(socket, boost::asio::buffer(response), error);
      if (error) {
          std::cerr << "Failed to write to socket. Error: " << error.message() << std::endl;
          return;
      }
      std::cout << "Updated position for " << json["id"] << " to (" << x << ", " << z << ")\n";
    } else {
      std::cout << "X and Z keys not found or incorrect type in received data" << std::endl;
      return;
    }
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << '\n';
  }
}

void client_handler(boost::asio::ip::tcp::socket socket) {
  playerCounter++;
  std::cout << "A player has connected. Total players: " << playerCounter << std::endl;

  std::array<char, 128> buf;
  boost::system::error_code error;

  for (;;) {
    size_t len = socket.read_some(boost::asio::buffer(buf), error);
    if (error == boost::asio::error::eof) break;
    else if (error) throw boost::system::system_error(error);

    std::string data(buf.begin(), buf.begin() + len);
    process_data(data, socket);
  }

  playerCounter--;
  std::cout << "A player has disconnected. Remaining players: " << playerCounter << std::endl;
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
