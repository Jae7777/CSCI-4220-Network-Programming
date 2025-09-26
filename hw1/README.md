Justin Chen, Suyash Amatya

09/26/2025

CSCI-4220 Network Programming

This TFTP server implements the requirements specified in RFC 1350 for "octet" mode. It supports concurrent connections by forking a new process for each client request. Timeouts and retransmissions are handled using SIGALRM, as required. A 1-second timer is used for retransmissions, and the connection is aborted after 10 unsuccessful retries. The server binds to the first port in a given range and assigns subsequent ports to child processes for data transfer.

### Compiling
1. Have the unpv13e-master directory cloned
2. Set the path to the unpv13e-master directory in the makefile
3. Run `make` to compile the server. This will create an executable named `tftp.out`.
4. Run the server with a port range, for example: `./tftp.out 9000 9010`

### Testing download using TFTP client
1. Start the server: `./tftp.out 9000 9010`
2. In another terminal, connect to the server and get the file:
```
tftp localhost 9000
tftp> mode binary
tftp> get tests/test1.txt my_downloaded_file.txt
```
3.  Verify that `my_downloaded_file.txt` has the same content as `test.txt`.

This works with any file formats.

### Testing upload (Write Request - Upload to server):
1.  Start the server: `./tftp.out 9000 9010`
2.  In another terminal, connect to the server and put a file:
    ```
    tftp localhost 9000
    tftp> mode binary
    tftp> put test.txt uploaded_file.txt
    ```
3.  Verify that `uploaded_file.txt` has been created in the `hw1` directory with the correct content.
