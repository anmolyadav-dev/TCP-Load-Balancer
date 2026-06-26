"""
Simple TCP echo server to test the load balancer.
Echoes back whatever the client sends, prefixed with the port number
so you can see which backend handled the request.

Usage: python3 test_backend.py <port>
"""

import sys, socket, threading

def handle(conn, port):
    with conn:
        while (data := conn.recv(4096)):
            conn.sendall(f"[backend:{port}] {data.decode()}".encode())

port = int(sys.argv[1])
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("", port))
srv.listen()
print(f"Backend listening on :{port}")

while True:
    conn, _ = srv.accept()
    threading.Thread(target=handle, args=(conn, port), daemon=True).start()
