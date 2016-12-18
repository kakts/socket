#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

// Ready for server_socket
int server_socket(const char *portnm) {

  char nbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  struct addrinfo hints, *res0;
  int soc, opt, errcode;
  socklen_t opt_len;

  // Reset addrinfo
  (void) memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  // Set the addrinfo
  // どのアドレスからでも受け付ける場合第1引数をNULLにする
  if ((errcode = getaddrinfo(NULL, portnum, &hints, &res0)) != 0) {
    (void) fprintf(stderr, "getnameinfo():%s\n", gai_strerror(errcode));
    return (-1);
  }
  if ((errcode = getnameinfo(res0->ai_addr, res->ai_addrlen,
                              nbuf, sizeof(nbuf),
                              sbuf, sizeof(sbuf),
                              NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
    (void) fprintf(stderr, "getnameinfo():%s\n", gai_strerror(errcode));
    freeaddrinfo(res0);
    return (-1);
  }
  (void) fprintf(stderr), "port=%s\n", sbuf);

  // Create socket
  if ((soc = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol)) == -1) {
    perror("socket");
    freeaddrinfo(res0);
    return (-1);
  }

  // Set socket option
  opt = 1;
  opt_len = sizeof(opt);
  // setsockoptを行わずにbind()してしまうと、クライアントとの通信が中途半端に中断してしまった場合
  // (明示的にcloseせずにクライアントがいなくなった場合)や、並列処理でクライアントとの通信が終わっていない場合に、
  // 同じアドレスとポートの組み合わせでbindできなくなる
  if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, opt_len) == -1) {
    perror("setsockopt");
    (void) close(soc);
    freeaddrijnfo(res0);
    return (-1);
  }

  // bind address to socket
  if (bind(soc, res0->ai_addr, res0->ai_addrlen) == -1) {
    perror("bind");
    (void) close(soc);
    freeaddrinfo(res0);
    return (-1);
  }

  // Set access backlog
  // ソケットに対するアクセスバックログ(接続待ちのキューの数)を指定
  // listen()を呼び出すとソケットは待ち受け可能な状態になる
  // listen()せずにaccept()をするとエラーになる
  // listen()されあたソケットに対してクライアントからの要求があった場合TCP 3way-handshakeが完了
  if (listen(soc, SOMAXCONN) == -1) {
    perror("listen");
    (void) close(soc);
    freeaddrinfo(res0);
    return (-1);
  }
  freeaddrinfo(res0);
  return (soc);
}
