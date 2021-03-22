// Copyright (c) 2021 CloudMakers, s. r. o.
// All rights reserved.
//
// You can use this software under the terms of 'INDIGO Astronomy
// open-source license' (see LICENSE.md).
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS 'AS IS' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// version history
// 2.0 by Peter Polakovic <peter.polakovic@cloudmakers.eu>

/** INDIGO ASCOM ALPACA bridge agent
 \file indigo_agent_alpaca.c
 */

#define DRIVER_VERSION 0x0001
#define DRIVER_NAME	"indigo_agent_alpaca"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <indigo/indigo_bus.h>
#include <indigo/indigo_io.h>
#include <indigo/indigo_server_tcp.h>

#include "indigo_agent_alpaca.h"
#include "alpaca_common.h"

//#define INDIGO_PRINTF(...) if (!indigo_printf(__VA_ARGS__)) goto failure

#define PRIVATE_DATA													private_data

#define AGENT_DISCOVERY_PROPERTY							(PRIVATE_DATA->discovery_property)
#define AGENT_DISCOVERY_PORT_ITEM							(AGENT_DISCOVERY_PROPERTY->items+0)

#define AGENT_DEVICES_PROPERTY								(PRIVATE_DATA->devices_property)

#define DISCOVERY_REQUEST											"alpacadiscovery1"
#define DISCOVERY_RESPONSE										"{ \"AlpacaPort\":%d }"

typedef struct {
	indigo_property *discovery_property;
	indigo_property *devices_property;
	pthread_mutex_t mutex;
} agent_private_data;

static agent_private_data *private_data = NULL;

static int discovery_server_socket = 0;
static indigo_alpaca_device *alpaca_devices = NULL;
static uint32_t server_transaction_id = 0;

indigo_device *indigo_agent_alpaca_device = NULL;
indigo_client *indigo_agent_alpaca_client = NULL;


// -------------------------------------------------------------------------------- ALPACA bridge implementation

static void start_discovery_server(indigo_device *device) {
	int port = (int)AGENT_DISCOVERY_PORT_ITEM->number.value;
	discovery_server_socket = socket(PF_INET, SOCK_DGRAM, 0);
	if (discovery_server_socket == -1) {
		INDIGO_DRIVER_ERROR(DRIVER_NAME, "Failed to create socket (%s)", strerror(errno));
		return;
	}
	int reuse = 1;
	if (setsockopt(discovery_server_socket, SOL_SOCKET,SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		close(discovery_server_socket);
		INDIGO_DRIVER_ERROR(DRIVER_NAME, "setsockopt() failed (%s)", strerror(errno));
		return;
	}
	struct sockaddr_in server_address;
	unsigned int server_address_length = sizeof(server_address);
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(discovery_server_socket, (struct sockaddr *)&server_address, server_address_length) < 0) {
		close(discovery_server_socket);
		INDIGO_DRIVER_ERROR(DRIVER_NAME, "bind() failed (%s)", strerror(errno));
		return;
	}
	INDIGO_DRIVER_LOG(DRIVER_NAME, "Discovery server started on port %d", port);
	fd_set readfd;
	struct sockaddr_in client_address;
	unsigned int client_address_length = sizeof(client_address);
	char buffer[128];
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	while (discovery_server_socket) {
		FD_ZERO(&readfd);
		FD_SET(discovery_server_socket, &readfd);
		int ret = select(discovery_server_socket + 1, &readfd, NULL, NULL, &tv);
		if (ret > 0) {
			if (FD_ISSET(discovery_server_socket, &readfd)) {
				recvfrom(discovery_server_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_address, &client_address_length);
				if (strstr(buffer, DISCOVERY_REQUEST)) {
					INDIGO_DRIVER_LOG(DRIVER_NAME, "Discovery request from %s", inet_ntoa(client_address.sin_addr));
					sprintf(buffer, DISCOVERY_RESPONSE, indigo_server_tcp_port);
					sendto(discovery_server_socket, buffer, strlen(buffer), 0, (struct sockaddr*)&client_address, client_address_length);
				}
			}
		}
	}
	INDIGO_DRIVER_LOG(DRIVER_NAME, "Discovery server stopped on port %d", port);
	return;
}

static void shutdown_discovery_server() {
	if (discovery_server_socket) {
		shutdown(discovery_server_socket, SHUT_RDWR);
		close(discovery_server_socket);
		discovery_server_socket = 0;
	}
}

static void parse_url_params(char *params, uint32_t *client_id, uint32_t *client_transaction_id) {
	if (params == NULL)
		return;
	while (true) {
		char *token = strtok_r(params, "&", &params);
		if (token == NULL)
			break;
		if (!strncmp(token, "ClientID", 8)) {
			if ((token = strchr(token, '='))) {
				*client_id = (uint32_t)atol(token + 1);
			}
		} else if (!strncmp(token, "ClientTransactionID", 19)) {
			if ((token = strchr(token, '='))) {
				*client_transaction_id = (uint32_t)atol(token + 1);
			}
		}
	}
}

static void send_json_response(int socket, char *path, int status_code, const char *status_text, char *body) {
	if (indigo_printf(socket,
			"HTTP/1.1 %3d %s\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n"
			"\r\n"
			"%s", status_code, status_text, strlen(body), body)) {
		if (status_code == 200)
			INDIGO_DRIVER_DEBUG(DRIVER_NAME, "%s -> 200 %s", path, status_text);
		else
			INDIGO_DRIVER_ERROR(DRIVER_NAME, "%s -> %3d %s", path, status_code, status_text);
		INDIGO_DRIVER_TRACE(DRIVER_NAME, "%s", body);
	} else {
		INDIGO_DRIVER_ERROR(DRIVER_NAME, "% -> Failed", path);
	}
}

static void send_text_response(int socket, char *path, int status_code, const char *status_text, char *body) {
	if (indigo_printf(socket,
			"HTTP/1.1 %3d %s\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: %d\r\n"
			"\r\n"
			"%s", status_code, status_text, strlen(body), body)) {
		if (status_code == 200)
			INDIGO_DRIVER_DEBUG(DRIVER_NAME, "%s -> 200 %s", path, status_text);
		else
			INDIGO_DRIVER_ERROR(DRIVER_NAME, "%s -> %3d %s", path, status_code, status_text);
		INDIGO_DRIVER_TRACE(DRIVER_NAME, "%s", body);
	} else {
		INDIGO_DRIVER_ERROR(DRIVER_NAME, "% -> Failed", path);
	}
}

static void alpaca_setup_handler(int socket, char *method, char *path, char *params) {
	if (indigo_printf(socket,
			"HTTP/1.1 301 Moved Permanently\r\n"
			"Location: /mng.html\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 0\r\n"
			"\r\n"
		))
		INDIGO_DRIVER_DEBUG(DRIVER_NAME, "% -> OK", path);
	else
		INDIGO_DRIVER_LOG(DRIVER_NAME, "% -> Failed", path);
}

static void alpaca_apiversions_handler(int socket, char *method, char *path, char *params) {
	uint32_t client_id = 0, client_transaction_id = 0;
	char buffer[128];
	parse_url_params(params, &client_id, &client_transaction_id);
	snprintf(buffer, sizeof(buffer), "{ \"Value\": [ 1 ], \"ClientTransactionID\": %u, \"ServerTransactionID\": %u }", client_transaction_id, server_transaction_id++);
	send_json_response(socket, path, 200, "OK", buffer);
}

static void alpaca_v1_description_handler(int socket, char *method, char *path, char *params) {
	uint32_t client_id = 0, client_transaction_id = 0;
	char buffer[512];
	parse_url_params(params, &client_id, &client_transaction_id);
	snprintf(buffer, sizeof(buffer), "{ \"Value\": { \"ServerName\": \"INDIGO-ALPACA Bridge\", \"ServerVersion\": \"%d.%d-%s\", \"Manufacturer\": \"The INDIGO Initiative\", \"ManufacturerURL\": \"https://www.indigo-astronomy.org\" }, \"ClientTransactionID\": %u, \"ServerTransactionID\": %u }", (INDIGO_VERSION_CURRENT >> 8) & 0xFF, INDIGO_VERSION_CURRENT & 0xFF, INDIGO_BUILD, client_transaction_id, server_transaction_id++);
	send_json_response(socket, path, 200, "OK", buffer);
}

#define BUFFER_SIZE (128 * 1024)

static void alpaca_v1_configureddevices_handler(int socket, char *method, char *path, char *params) {
	uint32_t client_id = 0, client_transaction_id = 0;
	char *buffer = indigo_safe_malloc(BUFFER_SIZE);
	parse_url_params(params, &client_id, &client_transaction_id);
	long index = snprintf(buffer, BUFFER_SIZE, "{ \"Value\": [ ");
	indigo_alpaca_device *alpaca_device = alpaca_devices;
	while (alpaca_device) {
		if (alpaca_device->device_type) {
			index += snprintf(buffer + index, BUFFER_SIZE - index, "{ \"DeviceName\": \"%s\", \"DeviceType\": \"%s\", \"DeviceNumber\": \"%d\", \"UniqueID\": \"%s\" }", alpaca_device->device_name, alpaca_device->device_type, alpaca_device->device_number, alpaca_device->device_uid);
			alpaca_device = alpaca_device->next;
			if (alpaca_device)
				buffer[index++] = ',';
			buffer[index++] = ' ';
		} else {
			alpaca_device = alpaca_device->next;
		}
	}
	snprintf(buffer + index, BUFFER_SIZE - index, "], \"ClientTransactionID\": %u, \"ServerTransactionID\": %u }", client_transaction_id, server_transaction_id++);
	send_json_response(socket, path, 200, "OK", buffer);
	free(buffer);
}

int string_cmp(const void * a, const void * b) {
	 return strncasecmp((char *)a, (char *)b, 128);
}

static void alpaca_v1_api_handler(int socket, char *method, char *path, char *params) {
	INDIGO_DRIVER_DEBUG(DRIVER_NAME, "< %s %s %s", method, path, params);
	uint32_t client_id = 0, client_transaction_id = 0;
	char *device_type = strstr(path, "/api/v1/");
	if (device_type == NULL) {
		send_text_response(socket, path, 400, "Bad Request", "Wrong API prefix");
		return;
	}
	device_type += 8;
	char *device_number = strchr(device_type, '/');
	if (device_number == NULL) {
		send_text_response(socket, path, 400, "Bad Request", "Missing device type");
		return;
	}
	*device_number++ = 0;
	char *command = strchr(device_number, '/');
	if (command == NULL) {
		send_text_response(socket, path, 400, "Bad Request", "Missing device number");
		return;
	}
	*command++ = 0;
	char *buffer = indigo_safe_malloc(BUFFER_SIZE);
	indigo_alpaca_device *alpaca_device = alpaca_devices;
	uint32_t number = (uint32_t)atol(device_number);
	while (alpaca_device) {
		if (alpaca_device->device_number == number) {
			if (alpaca_device->device_type && !strcasecmp(alpaca_device->device_type, device_type)) {
				break;
			}
			send_text_response(socket, path, 400, "Bad Request", "Device type doesn't match");
			return;
		}
		alpaca_device = alpaca_device->next;
	}
	if (alpaca_device == NULL) {
		send_text_response(socket, path, 400, "Bad Request", "No such device");
		return;
	}
	if (!strcmp(method, "GET")) {
		parse_url_params(params, &client_id, &client_transaction_id);
		long index = snprintf(buffer, BUFFER_SIZE, "{ ");
		long length = indigo_alpaca_get_command(alpaca_device, 1, command, buffer + index, BUFFER_SIZE - index);
		if (length > 0) {
			index += length;
			snprintf(buffer + index, BUFFER_SIZE - index, ", \"ClientTransactionID\": %u, \"ServerTransactionID\": %u }", client_transaction_id, server_transaction_id++);
			INDIGO_DRIVER_DEBUG(DRIVER_NAME, "> %s", buffer);
			send_json_response(socket, path, 200, "OK", buffer);
		} else {
			send_text_response(socket, path, 400, "Bad Request", "Unrecognised command");
		}
	} else if (!strcmp(method, "PUT")) {
		int content_length = 0;
		while (indigo_read_line(socket, buffer, BUFFER_SIZE) > 0) {
			if (!strncasecmp(buffer, "Content-Length:", 15)) {
				content_length = atoi(buffer + 15);
			}
		}
		indigo_read_line(socket, buffer, content_length);
		buffer[content_length] = 0;
		INDIGO_DRIVER_DEBUG(DRIVER_NAME, "< %s", buffer);
		char *params = buffer;
		char args[5][128] = { 0 };
		int count = 0;
		while (true) {
			char *token = strtok_r(params, "&", &params);
			if (token == NULL)
				break;
			if (!strncmp(token, "ClientID", 8)) {
				if ((token = strchr(token, '='))) {
					client_id = (uint32_t)atol(token + 1);
				}
			} else if (!strncmp(token, "ClientTransactionID", 19)) {
				if ((token = strchr(token, '='))) {
					client_transaction_id = (uint32_t)atol(token + 1);
				}
			} else if (count < 5) {
				strncpy(args[count++], token, 128);
			}
		}
		if (count > 1)
			qsort(args, count, 128, string_cmp);
		long index = snprintf(buffer, BUFFER_SIZE, "{ ");
		long length = indigo_alpaca_set_command(alpaca_device, 1, command, buffer + index, BUFFER_SIZE - index, args[0], args[1]);
		if (length > 0) {
			index += length;
			snprintf(buffer + index, BUFFER_SIZE - index, ", \"ClientTransactionID\": %u, \"ServerTransactionID\": %u }", client_transaction_id, server_transaction_id++);
			INDIGO_DRIVER_DEBUG(DRIVER_NAME, "> %s", buffer);
			send_json_response(socket, path, 200, "OK", buffer);
		} else {
			send_text_response(socket, path, 400, "Bad Request", "Unrecognised command");
		}

	} else {
		send_text_response(socket, path, 400, "Bad Request", "Invalid method");
	}
	free(buffer);
}

// -------------------------------------------------------------------------------- INDIGO agent device implementation

static indigo_result agent_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property);

static indigo_result agent_device_attach(indigo_device *device) {
	assert(device != NULL);
	assert(PRIVATE_DATA != NULL);
	if (indigo_device_attach(device, DRIVER_NAME, DRIVER_VERSION, INDIGO_INTERFACE_AGENT) == INDIGO_OK) {
		// --------------------------------------------------------------------------------
		AGENT_DISCOVERY_PROPERTY = indigo_init_number_property(NULL, device->name, "AGENT_ALPACA_DISCOVERY", MAIN_GROUP, "Discovery Configuration", INDIGO_OK_STATE, INDIGO_RW_PERM, 1);
		if (AGENT_DISCOVERY_PROPERTY == NULL)
			return INDIGO_FAILED;
		indigo_init_number_item(AGENT_DISCOVERY_PORT_ITEM, "PORT", "Discovery port", 0, 0xFFFF, 0, 32227);
		AGENT_DEVICES_PROPERTY = indigo_init_text_property(NULL, device->name, "AGENT_ALPACA_DEVICES", MAIN_GROUP, "Device mapping", INDIGO_OK_STATE, INDIGO_RO_PERM, INDIGO_MAX_ITEMS);
		if (AGENT_DISCOVERY_PROPERTY == NULL)
			return INDIGO_FAILED;
		AGENT_DEVICES_PROPERTY->count = 0;

		// --------------------------------------------------------------------------------
		srand((unsigned)time(0));
		indigo_set_timer(device, 0, start_discovery_server, NULL);
		indigo_server_add_handler("/setup", &alpaca_setup_handler);
		indigo_server_add_handler("/management/apiversions", &alpaca_apiversions_handler);
		indigo_server_add_handler("/management/v1/description", &alpaca_v1_description_handler);
		indigo_server_add_handler("/management/v1/configureddevices", &alpaca_v1_configureddevices_handler);
		indigo_server_add_handler("/api/v1", &alpaca_v1_api_handler);
		CONNECTION_PROPERTY->hidden = true;
		CONFIG_PROPERTY->hidden = true;
		PROFILE_PROPERTY->hidden = true;
		pthread_mutex_init(&PRIVATE_DATA->mutex, NULL);
		INDIGO_DEVICE_ATTACH_LOG(DRIVER_NAME, device->name);
		return agent_enumerate_properties(device, NULL, NULL);
	}
	return INDIGO_FAILED;
}

static indigo_result agent_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property) {
	if (client == indigo_agent_alpaca_client)
		return INDIGO_OK;
	if (indigo_property_match(AGENT_DISCOVERY_PROPERTY, property))
		indigo_define_property(device, AGENT_DISCOVERY_PROPERTY, NULL);
	if (indigo_property_match(AGENT_DEVICES_PROPERTY, property))
		indigo_define_property(device, AGENT_DEVICES_PROPERTY, NULL);
	return indigo_device_enumerate_properties(device, client, property);
}

static indigo_result agent_change_property(indigo_device *device, indigo_client *client, indigo_property *property) {
	assert(device != NULL);
	assert(DEVICE_CONTEXT != NULL);
	assert(property != NULL);
	if (client == indigo_agent_alpaca_client)
		return INDIGO_OK;
	if (indigo_property_match(AGENT_DISCOVERY_PROPERTY, property)) {
		indigo_property_copy_values(AGENT_DISCOVERY_PROPERTY, property, false);
		shutdown_discovery_server();
		indigo_set_timer(device, 0, start_discovery_server, NULL);
		AGENT_DISCOVERY_PROPERTY->state = INDIGO_OK_STATE;
		indigo_update_property(device, AGENT_DISCOVERY_PROPERTY, NULL);
	}
	return indigo_device_change_property(device, client, property);
}

static indigo_result agent_device_detach(indigo_device *device) {
	assert(device != NULL);
	shutdown_discovery_server();
	indigo_server_remove_resource("/setup");
	indigo_release_property(AGENT_DISCOVERY_PROPERTY);
	indigo_release_property(AGENT_DEVICES_PROPERTY);
	pthread_mutex_destroy(&PRIVATE_DATA->mutex);
	return indigo_device_detach(device);
}

// -------------------------------------------------------------------------------- INDIGO agent client implementation

static void update_devices_property(indigo_device *device) {
	indigo_delete_property(device, AGENT_DEVICES_PROPERTY, NULL);
	int count = 0;
	indigo_alpaca_device *alpaca_device = alpaca_devices;
	while (alpaca_device) {
		if (alpaca_device->device_type) {
			indigo_item *item = AGENT_DEVICES_PROPERTY->items + count++;
			sprintf(item->name, "%d", alpaca_device->device_number);
			sprintf(item->label, "%s/%d", alpaca_device->device_type, alpaca_device->device_number);
			strcpy(item->text.value, alpaca_device->indigo_device);
		}
		alpaca_device = alpaca_device->next;
	}
	AGENT_DEVICES_PROPERTY->count = count;
	indigo_define_property(device, AGENT_DEVICES_PROPERTY, NULL);
}

static indigo_result agent_define_property(indigo_client *client, indigo_device *device, indigo_property *property, const char *message) {
	indigo_alpaca_device *alpaca_device = alpaca_devices;
	while (alpaca_device) {
		if (!strcmp(property->device, alpaca_device->indigo_device))
			break;
		alpaca_device = alpaca_device->next;
	}
	if (alpaca_device == NULL) {
		static uint32_t device_number = 0;
		unsigned char digest[15] = { 0 };
		for (int i = 0, j = 0; property->device[i]; i++, j = (j + 1) % 15) {
			digest[j] = digest[j] ^ property->device[i];
		}
		alpaca_device = indigo_safe_malloc(sizeof(indigo_alpaca_device));
		strcpy(alpaca_device->indigo_device, property->device);
		alpaca_device->device_number = device_number++;
		strcpy(alpaca_device->device_uid, "xxxxxxxx-xxxx-4xxx-8xxx-xxxxxxxxxxxx");
		static char *hex = "0123456789ABCDEF";
		int i = 0;
		for (char *c = alpaca_device->device_uid; *c; c++) {
			int r = i % 2 == 0 ? digest[i / 2] % 16 : digest[i / 2] / 16;
			switch (*c) {
				case 'x':
					*c = hex[r];
					break;
				default:
					break;
			}
			i++;
		}
		pthread_mutex_init(&alpaca_device->mutex, NULL);
		alpaca_device->next = alpaca_devices;
		alpaca_devices = alpaca_device;
	}
	if (!strcmp(property->name, INFO_PROPERTY_NAME)) {
		for (int i = 0; i < property->count; i++) {
			indigo_item *item = property->items + i;
			if (!strcmp(item->name, INFO_DEVICE_INTERFACE_ITEM_NAME)) {
				uint64_t interface = atoll(item->text.value);
				switch (interface) {
					case INDIGO_INTERFACE_CCD:
						alpaca_device->device_type = "Camera";
						break;
					case INDIGO_INTERFACE_DOME:
						alpaca_device->device_type = "Dome";
						break;
					case INDIGO_INTERFACE_WHEEL:
						alpaca_device->device_type = "FilterWheel";
						break;
					case INDIGO_INTERFACE_FOCUSER:
						alpaca_device->device_type = "Focuser";
						break;
					case INDIGO_INTERFACE_ROTATOR:
						alpaca_device->device_type = "Rotator";
						break;
					case INDIGO_INTERFACE_AUX_POWERBOX:
						alpaca_device->device_type = "Switch";
						break;
					case INDIGO_INTERFACE_AO:
					case INDIGO_INTERFACE_MOUNT:
					case INDIGO_INTERFACE_GUIDER:
						alpaca_device->device_type = "Telescope";
						break;
					case INDIGO_INTERFACE_AUX_LIGHTBOX:
						alpaca_device->device_type = "CoverCalibrator";
						break;
					default:
						alpaca_device->device_type = NULL;
						interface = 0;
				}
				if (alpaca_device->device_type)
					update_devices_property(indigo_agent_alpaca_device);
			} else if (!strcmp(item->name, INFO_DEVICE_NAME_ITEM_NAME)) {
				pthread_mutex_lock(&alpaca_device->mutex);
				strcpy(alpaca_device->device_name, item->text.value);
				pthread_mutex_unlock(&alpaca_device->mutex);
			} else if (!strcmp(item->name, INFO_DEVICE_DRVIER_ITEM_NAME)) {
				pthread_mutex_lock(&alpaca_device->mutex);
				strcpy(alpaca_device->driver_info, item->text.value);
				pthread_mutex_unlock(&alpaca_device->mutex);
			} else if (!strcmp(item->name, INFO_DEVICE_VERSION_ITEM_NAME)) {
				pthread_mutex_lock(&alpaca_device->mutex);
				strcpy(alpaca_device->driver_version, item->text.value);
				pthread_mutex_unlock(&alpaca_device->mutex);
			}
		}
	} else {
		indigo_alpaca_update_property(alpaca_device, property);
	}
	return INDIGO_OK;
}

static indigo_result agent_update_property(indigo_client *client, indigo_device *device, indigo_property *property, const char *message) {
	indigo_alpaca_device *alpaca_device = alpaca_devices;
	while (alpaca_device) {
		if (!strcmp(property->device, alpaca_device->indigo_device)) {
			indigo_alpaca_update_property(alpaca_device, property);
			break;
		}
		alpaca_device = alpaca_device->next;
	}
	return INDIGO_OK;
}

static indigo_result agent_delete_property(indigo_client *client, indigo_device *device, indigo_property *property, const char *message) {
	indigo_alpaca_device *alpaca_device = alpaca_devices, *previous = NULL;
	while (alpaca_device) {
		if (!strcmp(property->device, alpaca_device->indigo_device)) {
			if (*property->name == 0 || !strcmp(property->name, CONNECTION_PROPERTY_NAME)) {
				if (previous == NULL) {
					alpaca_devices = alpaca_device->next;
				} else {
					previous->next = alpaca_device->next;
				}
				if (alpaca_device->device_type)
					update_devices_property(indigo_agent_alpaca_device);
				indigo_safe_free(alpaca_device);
			}
			break;
		}
		previous = alpaca_device;
		alpaca_device = alpaca_device->next;
	}
	return INDIGO_OK;
}

// -------------------------------------------------------------------------------- Initialization

indigo_result indigo_agent_alpaca(indigo_driver_action action, indigo_driver_info *info) {
	static indigo_device agent_device_template = INDIGO_DEVICE_INITIALIZER(
		ALPACA_AGENT_NAME,
		agent_device_attach,
		agent_enumerate_properties,
		agent_change_property,
		NULL,
		agent_device_detach
	);

	static indigo_client agent_client_template = {
		ALPACA_AGENT_NAME, false, NULL, INDIGO_OK, INDIGO_VERSION_CURRENT, NULL,
		NULL,
		agent_define_property,
		agent_update_property,
		agent_delete_property,
		NULL,
		NULL
	};

	static indigo_driver_action last_action = INDIGO_DRIVER_SHUTDOWN;

	SET_DRIVER_INFO(info, "ASCOM ALPACA bridge agent", __FUNCTION__, DRIVER_VERSION, false, last_action);

	if (action == last_action)
		return INDIGO_OK;

	switch(action) {
		case INDIGO_DRIVER_INIT:
			last_action = action;
			private_data = indigo_safe_malloc(sizeof(agent_private_data));
			indigo_agent_alpaca_device = indigo_safe_malloc_copy(sizeof(indigo_device), &agent_device_template);
			indigo_agent_alpaca_device->private_data = private_data;
			indigo_agent_alpaca_client = indigo_safe_malloc_copy(sizeof(indigo_client), &agent_client_template);
			indigo_agent_alpaca_client->client_context = indigo_agent_alpaca_device->device_context;
			indigo_attach_device(indigo_agent_alpaca_device);
			indigo_attach_client(indigo_agent_alpaca_client);
			break;

		case INDIGO_DRIVER_SHUTDOWN:
			last_action = action;
			if (indigo_agent_alpaca_client != NULL) {
				indigo_detach_client(indigo_agent_alpaca_client);
				free(indigo_agent_alpaca_client);
				indigo_agent_alpaca_client = NULL;
			}
			if (indigo_agent_alpaca_device != NULL) {
				indigo_detach_device(indigo_agent_alpaca_device);
				free(indigo_agent_alpaca_device);
				indigo_agent_alpaca_device = NULL;
			}
			if (private_data != NULL) {
				free(private_data);
				private_data = NULL;
			}
			break;

		case INDIGO_DRIVER_INFO:
			break;
	}
	return INDIGO_OK;
}
