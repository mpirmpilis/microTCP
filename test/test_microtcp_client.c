/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * You can use this file to write a test microTCP client.
 * This file is already inserted at the build system.
 */


#include "../lib/microtcp.h"
#include "../utils/crc32.h"

#define PORT 8080


/* run client from banana */
int main(int argc, char **argv) {
	microtcp_sock_t client_sock;
	int connection;
	struct sockaddr_in server_address; /* for server address */


	client_sock = microtcp_socket(AF_INET, SOCK_STREAM, 0);
	printf("Client socket:%d\n", client_sock.sd);


	server_address.sin_family = AF_INET;
	server_address.sin_port   = htons(PORT);
	server_address.sin_addr.s_addr = inet_addr("147.52.19.59");  /* kiwi IPv4 address -- will be the server */

	client_sock.address = server_address;
	client_sock.address_len = sizeof(struct sockaddr_in);

	connection = microtcp_connect(&client_sock, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in));
	if (connection == 0) {
		printf("Client connected to server\n");
	}


	return 0;

}
