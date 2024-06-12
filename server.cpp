#include "httprequestparser.h"
#include "request.h"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace httpparser;

#define BUFFER_SIZE 4097
#define MAX_CLIENTS 20
#define CACHE_SIZE 50

pthread_mutex_t lock;

struct cache_element
{
  /* data */
  char *url;
  char *response;
  size_t reslen;
  cache_element *next;
};

cache_element *head=nullptr;

void add_cache(char *url,char *response){
  pthread_mutex_lock(&lock);
  std::cout << url << std::endl;
  cache_element *element = new cache_element;
  element->url = strdup(url);
  std::cout << element->url;
  element->reslen = strlen(response);
  element->response = strdup(response);
  element->next = nullptr;
  if(head==nullptr){
    head = element;
    std::cout << "head created" << std::endl;
  }else{
    element->next = head;
    head=element;
    std::cout << "head updated" << std::endl;
  }
  std::cout << head->url << std::endl;
  pthread_mutex_unlock(&lock);
}

cache_element *find_element(char *url){
  
  if(head==nullptr){
    return nullptr;
  }

  if(strcmp(url,head->url)==0){
    return head;
  }
  cache_element *prev=head;
  cache_element *temp=head->next;
  while(temp!=nullptr){
    if(strcmp(url,temp->url)==0){
      cache_element *elem = new cache_element;
      elem->url = strdup(temp->url);
      elem->response = strdup(temp->response);
      elem->reslen = temp->reslen;
      elem->next = head;
      head = elem;
      prev->next= temp->next;
      return temp;
    }
    prev=temp;
    temp=temp->next;
  }
  return nullptr;
}

void print_cache(){
  cache_element *temp=head;
  while(temp!=nullptr){
    std::cout<<temp->url<<std::endl;
    temp=temp->next;
  }
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

static size_t HeaderCallback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::string get_data_from_remote_server(std::string url) {
  CURL *curl;
  CURLcode *res;
  std::string read_buffer;
  std::string header_buffer;
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_buffer);

  if (curl_easy_perform(curl) == CURLE_OK) {
    std::cout << "Received data from remote server" << std::endl;
  } else {
    std::cout << "Error receiving data from remote server" << std::endl;
    return "";
  }
   curl_easy_cleanup(curl);
  return header_buffer + read_buffer;
}

int HTTP_version_check(int a, int b) {
  if (a == 1 && (b == 0 || b == 1)) {
    return 1;
  }
  return 0;
}

void *handle_connections(void *args) {

  int client_socket = *((int *)args);

  char *buffer = new char[BUFFER_SIZE];
  int len = 0, bytes_received;
  memset(buffer, 0, BUFFER_SIZE);
  int i = 0;
  while (1) {
    std::cout << "loop runs " << ++i << " times" << std::endl;
    len = len + sizeof(buffer) - 8;
    bytes_received +=
        recv(client_socket, buffer + len, BUFFER_SIZE - 1 - len, 0);

    if (strstr(buffer, "\r\n\r\n") != NULL) {
      break;
    }
  }
  buffer[bytes_received] = '\0';

  Request req;
  HttpRequestParser parser;

  HttpRequestParser::ParseResult res =
      parser.parse(req, buffer, buffer + bytes_received);

  if (res == HttpRequestParser::ParsingCompleted) {
    std::cout << "Parsing complete." << std::endl;
  } else {
    std::cerr << "Parsing failed" << std::endl;
  }
  std::string url = req.uri.substr(9);
  std::cout << "req method " << req.method << " req.uri " << req.uri << " url "
            << url << std::endl;

  if(req.uri!="/favicon.ico")
  {if (HTTP_version_check(req.versionMajor, req.versionMinor) == 0) {
    std::cerr << "HTTP version not supported" << std::endl;
    return NULL;
  }
 
  cache_element *check = find_element((char *)url.c_str());
  if(check!=nullptr){
    std::cout << "cache hit"<<std::endl;
    if (send(client_socket, check->response, check->reslen, 0) < 0) {
    std::cerr << "Error sending response to client" << std::endl;
    // close(client_socket);
  }
  }
  else{
    std::string response = get_data_from_remote_server(url);
  if (response == "") {
    std::cerr << "No response received" << std::endl;
    return NULL;
  }
  add_cache((char *)url.c_str(),(char *)response.c_str());
  if (send(client_socket, response.c_str(), response.size(), 0) < 0) {
    std::cerr << "Error sending response to client" << std::endl;
    // close(client_socket);
  }
  //}
  std::cout << "printing cache" << std::endl;
  print_cache();
  }}
  close(client_socket);
  return NULL;
}

int main(int argc, char *argv[]) {

  if (argc != 2) {
    std::cerr << "Please provide port number" << std::endl;
    exit(1);
  }

  int proxy = socket(AF_INET, SOCK_STREAM, 0);

  if (proxy < 0) {
    std::cerr << "Error creating socket,exiting the program" << std::endl;
    exit(1);
  }
  std::cout << "Socket created" << std::endl;

  sockaddr_in proxy_address, client_address;
  size_t proxy_len = sizeof(proxy_address);
  socklen_t client_len = sizeof(client_address);
  memset(&proxy_address, 0, proxy_len);
  proxy_address.sin_port = htons(std::atoi(argv[1]));
  proxy_address.sin_family = AF_INET;
  proxy_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(proxy, (struct sockaddr *)&proxy_address, proxy_len) < 0) {
    std::cerr << "Error binding socket,exiting the program" << std::endl;
    exit(1);
  }
  std::cout << "Socket bound to port " << ntohs(proxy_address.sin_port)
            << std::endl;
  if (listen(proxy, MAX_CLIENTS) < 0) {
    std::cerr << "Error listening to connections ,exiting the program"
              << std::endl;
    exit(1);
  }
  std::cout << " Listening on port " << ntohs(proxy_address.sin_port)
            << std::endl;
  int client_index = 0;
  while (1) {
    memset(&client_address, 0, sizeof(client_address));
    int client_socket =
        accept(proxy, (struct sockaddr *)&client_address, &client_len);

    if (client_socket < 0) {
      std::cerr << "Error accepting connection" << std::endl;
    }
    client_index++;
    if (client_index > MAX_CLIENTS) {
      std::cerr << "Max client limit exceeded" << std::endl;
      break;
    }
    in_addr ip_addr = client_address.sin_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
    std::cout << "Client connected from " << ntohs(client_address.sin_port)
              << "and address " << str << std::endl;
    pthread_t t;
    pthread_create(&t, NULL, handle_connections, &client_socket);
  }

  return 0;
}
