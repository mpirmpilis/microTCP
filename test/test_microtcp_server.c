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
 * You can use this file to write a test microTCP server.
 * This file is already inserted at the build system.
 */


#include "../lib/microtcp.h"
#include "../utils/crc32.h"

#define PORT 8080


/* run server from kiwi */
int main(int argc, char **argv) {
	microtcp_sock_t server_sock;
	int bind_res, accept_res;
	struct sockaddr_in server_address; /* for server address */


	server_sock = microtcp_socket(AF_INET, SOCK_STREAM, 0);
	printf("Server socket:%d\n", server_sock.sd);
	

	server_address.sin_family = AF_INET;
	server_address.sin_port   = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	server_sock.address = server_address;
	server_sock.address_len = sizeof(struct sockaddr_in);

	bind_res = microtcp_bind(&server_sock, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in));
	if (bind_res == 0) {
		printf("Binding successfull\n");
	} else {
		printf("Error in binding!\n");
	}



	accept_res = microtcp_accept(&server_sock, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in));
	if (accept_res == 0) {
		printf("Accepting successfull\n");
	} else {
		printf("Error in accepting!\n");
	}


	return 0;
}
