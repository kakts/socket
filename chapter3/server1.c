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


// サイズ指定文字列連結
size_t mystrlcat(char *dst, const char *src, size_t size) {
  const char *ps;
  char *pd, *pde;
  size_t dlen, lest;

  for (pd = dst, lest = size; *pd != '\0' && lest != 0; pd++, lest--);
  dlen = pd - dst;
  if (size - dlen == 0) {
    return (dlen + strlen(src));
  }

  pde = dst + size - 1;
  for (ps = src; *ps != '\0' && pd < pde; pd++, ps++) {
    *pd = *ps;
  }
  for (; pd <= pde; pd++) {
    *pd = '\0';
  }
  while (*ps++);
  return (dlen + (ps - src - 1));
}

// 送受信ループ
void send_recv_loop(int acc) {
  char buf[512], *ptr;
  ssize_t len;

  for (;;) {
    // 受信
    if ((len = recv(acc, buf, sizeof(buf), 0)) == -1) {
      // Error
      perror("recv");
      break;
    }
    if (len == 0) {
      // end of file
      (void) fprintf(stderr, "recv:EOF\n");
      break;
    }

    // 文字列化・表示
    buf[len] = '\0';
    if ((ptr = strpbrk(buf, "\r\n")) != NULL) {
      *ptr = '\0';
    }

    (void) fprintf(stderr, "[client]%s\n", buf);

    // 応答文字列作成
    (void) mystrlcat(buf, ":OK\r\n", sizeof(buf));
    len = (ssize_t) strlen(buf);
    // 応答
    if ((len = send(acc, buf, (size_t) len, 0)) == -1) {
      // Error
      perror("send");
      break;
    }
  }
}

// Ready for server_socket
// chapter1と異なり、引数でホスト名またはIPアドレスを渡し、そのアドレスをbind()で
// バインドするように変更する
int server_socket_by_hostname(const char *hostnm, const char *portnm) {

  char nbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  struct addrinfo hints, *res0;
  int soc, opt, errcode;
  socklen_t opt_len;

  // Reset addrinfo
  (void) memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // AI_PASSIVE = ソケットがbind()を利用される予定であるという意味になる

  // Set the addrinfo
  // 第１引数にhostnmを指定する
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
  (void) fprintf(stderr, "addr=%s\n port=%s\n", nbuf, sbuf);

  // Create socket
  soc = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol);
  if (soc == -1) {
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
    freeaddrinfo(res0);
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
    perror("listen");
    (void) close(soc);
    freeaddrinfo(res0);
    return (-1);
  }
  freeaddrinfo(res0);
  return (soc);
}

// accept loop
// 並列処理を行っていないので、受け付けた後、1つのクライアントの送受信処理が終わるまで他の受付ができない。
void accept_loop(int soc) {
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  struct sockaddr_storage from;
  int acc;
  socklen_t len;


  for (;;) {
    len = (socklen_t) sizeof(from);

    // waiting connection
    // 1つも待ちがない状態だとaccept()実行でブロックする

    // ここでsocが０になっている
    acc = accept(soc, (struct sockaddr *) &from, &len);
    (void) fprintf(stderr, "before0 %d %d", acc, soc);
    if (acc == -1) {
          (void) fprintf(stderr, "before1 %d \n", errno);
      if (errno != EINTR) {
        perror("accept");
      }
    } else {
            (void) fprintf(stderr, "test1");
      (void) getnameinfo((struct sockaddr *) &from, len,
                          hbuf, sizeof(hbuf),
                          sbuf, sizeof(sbuf),
                          NI_NUMERICHOST | NI_NUMERICSERV);
      (void) fprintf(stderr, "acceptｎ:%s:%s\n", hbuf, sbuf);

      // loop
      (void) send_recv_loop(acc);
      // close
      (void) close(acc);
      acc = 0;
    }
  }
}

int main(int argc, char *argv[]) {
  int soc = 0;
  // check if port num and ip address are set to args
  if (argc <= 2) {
    (void) fprintf(stderr, "server port\n");
    return (EX_USAGE);
  }

  // Prepare for making server_socket
  if ((soc = server_socket_by_hostname(argv[1], argv[2])) == -1) { (void) fprintf(stderr, "server_socket(%s, %s):error\n", argv[1], argv[2]);
    return (EX_UNAVAILABLE);
  }
  (void) fprintf(stderr, "ready for accept\n");
  // accept loop
  accept_loop(soc);
  // close server_socket
  (void) close(soc);
  return (EX_OK);
}
