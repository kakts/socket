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
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

// Socket connection to server
int client_socket(const char *hostnm, const char *portnm) {
  char nbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  struct addrinfo hints, *res0;
  int soc, errcode;

  // Reset addrinfo
  (void) memset(&hints, 0, sizeof(hints));
  // ソケットサーバとは違って、ai_flagsを設定しない
  // IPV4をつかってsocket type にストリームを設定するのみ
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  // Set the addrinfo
  // 明示的に接続先のIPアドレス、ホスト名を指定する必要があるため、hostnm指定
  if ((errcode = getaddrinfo(hostnm, portnm, &hints, &res0)) != 0) {
    (void) fprintf(stderr, "getaddrinfo():%s\n", gai_strerror(errcode));
    return (-1);
  }
  if ((errcode = getnameinfo(res0->ai_addr, res0->ai_addrlen,
                              nbuf, sizeof(nbuf),
                              sbuf, sizeof(sbuf),
                              NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
    (void) fprintf(stderr, "getnameinfo():%s\n", gai_strerror(errcode));
    freeaddrinfo(res0);
    return (-1);
  }
  (void) fprintf(stderr, "addr=%s\n", nbuf);
  (void) fprintf(stderr, "port=%s\n", sbuf);

  // create a socket
  if ((soc = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol)) == -1) {
    perror("socket");
    freeaddrinfo(res0);
    return (-1);
  }
  // connect to the socket-server
  if (connect(soc, res0->ai_addr, res0->ai_addrlen) == -1) {
    perror("connect");
    (void) close(soc);
    freeaddrinfo(res0);
    return (-1);
  }
  freeaddrinfo(res0);
  return (soc);
}

// 送受信処理
void send_recv_loop(int soc) {
  char buf[512];
  struct timeval timeout;
  int end, width;
  ssize_t len;
  fd_set mask, ready;

  // select()用マスク
  // 最初にクリアする
  FD_ZERO(&mask);
  // ソケットディスクリプタをセット
  FD_SET(soc, &mask);
  // 標準入力をセット
  FD_SET(0, &mask);

  // selectの第１引数には最大のディスクリプタの値 + 1を指定する
  width = soc + 1;

  // 送受信
  for (end = 0;; ) {
    // マスクの代入
    // select()を呼び出すたびに、マスクの値が書き換わるので、毎回別の変数にコピーしてから渡す
    ready = mask;
    // タイムアウト値のセット
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    switch (select(width, (fd_set *) &ready, NULL, NULL, &timeout)) {
      case -1:
        // error
        perror("select");
        break;
      case 0:
        // timeout
        break;
      default:
        // socket ready
        if (FD_ISSET(soc, &ready)) {
          // 受信
          if ((len == recv(soc, buf, sizeof(buf), 0)) == -1) {
            // error
            perror("recv");
            end = 1;
            break;
          }
          if (len == 0) {
            // end of file
            (void) fprintf(stderr, "recv:EOF\n");
            end = 1;
            break;
          }

          // 文字列化・表示
          buf[len] = '\0';
          (void) printf("> %s", buf);
        }

        // stdin ready
        if (FD_ISSET(0, &ready)) {
          // read 1 line from stdin
          (void) fgets(buf, sizeof(buf), stdin);
          if (feof(stdin)) {
            end = 1;
            break;
          }

          // send
          if ((len = send(soc, buf, strlen(buf), 0)) == -1) {
            // error
            perror("send");
            end = 1;
            break;
          }
        }
        break;
    }
    if (end) {
      break;
    }
  }
}

int main(int argc, char *argv[]) {
  int soc;
  // 引数にホスト名・ポート番号が指定されているかチェック
  if (argc <= 2) {
    (void) fprintf(stderr, "client server-host port \n");
    return (EX_USAGE);
  }
  // Try socket connection to server
  if ((soc = client_socket(argv[1], argv[2])) == -1) {
    (void) fprintf(stderr, "client_socket():error\n");
    return (EX_UNAVAILABLE);
  }

  // 送受信処理
  send_recv_loop(soc);
  // close socket
  (void) close(soc);
  return (EX_OK);
}
