#include <iostream>
#include <ostream>
#include <istream>
#include <sstream>
#include <ctime>
#include <string>
#include <asio.hpp>
#include <functional>
#include <thread>
#include <random>
#include <iterator>

using asio::ip::tcp;
using namespace std::placeholders;

class HttpServer; // forward declaration

class Request : public std::enable_shared_from_this<Request> {
public:
  static std::string make_daytime_string() {
    std::time_t now = std::time(0);
    return std::ctime(&now);
  }

  Request(HttpServer& server, asio::io_service& io_service)
      : server(server), io_service(io_service), socket(io_service) {}

  void start() {
    // Read request until the end
    asio::async_read_until(socket, request, "\r\n\r\n",
                           std::bind(&Request::afterRead, shared_from_this(), _1, _2));
  }

  tcp::socket& get_socket() {
    return socket;
  }

  int get_randint() {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distint(1,100); // distribution in range [1, 6]
    return distint(rng);
  }

  double get_randfloat(double min = 0.0f, double max = 1.0f) {
      // Use a modern, well-regarded random number engine
      std::random_device rd;  // Non-deterministic random number generator
      std::mt19937 gen(rd());   // Seed the Mersenne Twister engine

      // Create a uniform real distribution for the desired range
      std::uniform_real_distribution<double> dis(min, max);

      // Generate a random double within the specified range
      return dis(gen);
  }

  bool get_randbool() {
    std::vector<bool> choices{true, false};
    return choices[get_randint() % 2];
  }

  // Function to convert C-style double array to JSON string
  std::string to_json(const double arr[], size_t size) {
    if (size == 0) {
      return "[]"; // Empty array
    }

    std::stringstream ss;
    ss << "[";

    // Use range-based for loop for cleaner iteration
    for (std::size_t i = 0; i < size; ++i) {
      ss << arr[i] << ", ";
    }

    // Remove the trailing comma and space
    ss.seekp(-2, std::ios_base::end);
    ss << "]";

    return ss.str();
  }

private:
  void afterRead(const std::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
      return;
    }

    // Parse request to identify the endpoint
    std::string request_string;
    std::copy(asio::buffers_begin(request.data()), asio::buffers_end(request.data()),
              std::back_inserter(request_string));

    std::string method;
    std::string path;
    std::stringstream request_stream(request_string);

    // Extract method and path from request
    std::getline(request_stream, method, ' ');
    std::getline(request_stream, path, '\n');

    // Prepare response based on the endpoint
    asio::streambuf response;
    std::ostream res_stream(&response);

    if (path.find("/getvalues") != std::string::npos) {
      // Handle /getvalueofa endpoint
      int a = get_randint(); // Replace with your desired value
      double b = get_randfloat(); // Replace with your desired value
      bool c = get_randbool();
      double d[5];
      for (std::size_t i = 0; i < 5; ++i) {
        d[i] = get_randfloat();
      }

      // Manually construct the JSON string with indentation
      std::stringstream ss;
      ss << "{\n";
      ss << "  \"a\": " << a << ",\n";
      ss << "  \"b\": " << b << ",\n";
      ss << "  \"c\": " << (c ? "true" : "false") << ",\n";
      ss << "  \"d\": " << to_json(d, 5) << "\n";
      ss << "}";

      // Prepare the HTTP response
      res_stream << "HTTP/1.0 200 OK\r\n"
                << "Content-Type: application/json\r\n"
                << "Content-Length: " << ss.str().length() << "\r\n\r\n"
                << ss.str();
    } else {
      std::string time = make_daytime_string();

      res_stream << "HTTP/1.0 200 OK\r\n"
              << "Content-Type: text/html; charset=UTF-8\r\n"
              << "Content-Length: " << time.length() + 206 << "\r\n\r\n"
              << "<html><head><title>Hello</title>"
              << "<script>"
              << "function updateValues() {"
              << "  var xhr = new XMLHttpRequest();"
              << "  xhr.open('GET', '/getvalues');"
              << "  xhr.onload = function() {"
              << "    if (xhr.status === 200) {"
              << "      document.getElementById('values').innerHTML = xhr.responseText;"
              << "    }"
              << "  };"
              << "  xhr.send();"
              << "}"
              << "setInterval(updateValues, 1000); // Update value every second"
              << "</script>"
              << "</head><body>"
              << "<pre><div id='values'/></pre>"
              << "</body></html>";
    }
    // Write response asynchronously
    asio::async_write(socket, response,
                      std::bind(&Request::afterWrite, shared_from_this(), _1, _2));
  }

  void afterWrite(const std::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
      return;
    }

    // Close connection after writing
    socket.close();
  }

  HttpServer& server;
  asio::io_service& io_service;
  asio::streambuf request;
  tcp::socket socket;
};

#include <chrono>

class HttpServer {
public:
  HttpServer(unsigned int port, asio::io_service& io_service)
      : acceptor(io_service, tcp::endpoint(tcp::v4(), port)), io_service(io_service) {}

  void initialize() {
    start_accept();
  }

  void step() {
    // Process any pending events from io_service
    io_service.poll();
    // Schedule another step for next iteration (assuming 80ms RTOS call)
    io_service.post([this] { });
  }

private:
  void start_accept() {
    std::shared_ptr<Request> req(new Request(*this, io_service));
    acceptor.async_accept(req->get_socket(),
                          std::bind(&HttpServer::handle_accept, this, req, _1));
  }

  void handle_accept(std::shared_ptr<Request> req, const std::error_code& error) {
    if (!error) {
      req->start(); // Start handling the request
    }
    start_accept(); // Continuously accept new connections
  }

  tcp::acceptor acceptor;
  asio::io_service& io_service;
};

asio::io_service io_service;

int main() {
  HttpServer server(8080, io_service);
  server.initialize();

  while (true) {
    server.step();
  }
}
