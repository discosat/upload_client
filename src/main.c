/**
 * Copied and edited from: https://github.com/spaceinventor/libcsp/blob/60e4804ea8451e6202ce2c5c5abc0342ad3b55a4/examples/csp_client.c
 */

#include <csp/csp_debug.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/interfaces/csp_if_zmqhub.h>

#include "vmem_dtp_server.h"

#include <dtp/dtp.h>
#include <dtp/dtp_log.h>
#include <dtp/dtp_session.h>

#define PORT 10

dtp_opt_session_hooks_cfg default_session_hooks;
extern dtp_opt_session_hooks_cfg apm_session_hooks;

typedef struct
{
	int color;
	int resume;
	uint32_t server;
	unsigned int throughput;
	unsigned int timeout;
	unsigned int payload_id;
	unsigned int mtu;
} dtp_client_opts_t;

// temp variable to only send once (DEBUG)
int hasSent = 0;

/* This function must be provided in arch specific way */
int router_start(void);

// file to be sent
const char *file_src = NULL;

void *router_task(void *param)
{
	while (1)
	{
		csp_route_work();
	}
	return NULL;
}

// IMPLEMENTATION of router_start
int router_start(void)
{
	pthread_t router_thread;
	if (pthread_create(&router_thread, NULL, router_task, NULL) != 0)
	{
		csp_print("Failed to start router thread\n");
		return -1;
	}
	csp_print("Router thread started\n");
	return 0;
}

// Struct to pass arguments to the DTP client thread
typedef struct
{
	uint32_t server_addr;
	FILE *output_file;

	int color;
	int resume;
	uint32_t server;
	unsigned int throughput;
	unsigned int timeout;
	unsigned int payload_id;
	unsigned int mtu;
} dtp_thread_args_t;

static void *dtp_client_worker(void *param)
{
	dtp_thread_args_t *opts = (dtp_thread_args_t *)param;
	dtp_t *session;

	csp_print("Starting DTP client for payload %u from server %u\n", opts->payload_id, opts->server_addr);

	// Run the DTP client. This will block until the transfer is complete or fails.
	dtp_result result = dtp_client_main(opts->server, opts->throughput, opts->timeout, opts->payload_id, opts->mtu, opts->resume, &session);

	if (result == DTP_ERR)
	{
		csp_print("DTP client failed: %s\n", dtp_strerror(dtp_errno(NULL)));
		// The on_end hook should be called by libdtp on failure to clean up resources.
	}
	else
	{
		csp_print("DTP client completed successfully.\n");
		dtp_release_session(session);
	}

	// Free the thread arguments
	free(opts);

	pthread_exit(NULL);
}

/* Server port, the port the server listens on for incoming connections from the client. */
#define SERVER_PORT 10

/* Commandline options */
static uint8_t server_address = 0;
static uint8_t client_address = 0;

/* Test mode, check that server & client can exchange packets */
static bool test_mode = false;
static unsigned int successful_ping = 0;
static unsigned int run_duration_in_sec = 3;

enum DeviceType
{
	DEVICE_UNKNOWN,
	DEVICE_CAN,
	DEVICE_KISS,
	DEVICE_ZMQ,
};

#define __maybe_unused __attribute__((__unused__))

static struct option long_options[] = {
	{"file_src", required_argument, 0, 'f'},
	{"kiss-device", required_argument, 0, 'k'},
#if (CSP_HAVE_LIBSOCKETCAN)
#define OPTION_c "c:"
	{"can-device", required_argument, 0, 'c'},
#else
#define OPTION_c
#endif
#if (CSP_HAVE_LIBZMQ)
#define OPTION_z "z:"
	{"zmq-device", required_argument, 0, 'z'},
#else
#define OPTION_z
#endif
#if (CSP_USE_RTABLE)
#define OPTION_R "R:"
	{"rtable", required_argument, 0, 'R'},
#else
#define OPTION_R
#endif
	{"interface-address", required_argument, 0, 'a'},
	{"connect-to", required_argument, 0, 'C'},
	{"test-mode", no_argument, 0, 't'},
	{"test-mode-with-sec", required_argument, 0, 'T'},
	{"help", no_argument, 0, 'h'},
	{0, 0, 0, 0}};

void print_help()
{
	csp_print("Usage: csp_client [options]\n");
	if (CSP_HAVE_LIBSOCKETCAN)
	{
		csp_print(" -c <can-device>  set CAN device\n");
	}
	if (1)
	{
		csp_print(" -k <kiss-device> set KISS device\n");
	}
	if (CSP_HAVE_LIBZMQ)
	{
		csp_print(" -z <zmq-device>  set ZeroMQ device\n");
	}
	if (CSP_USE_RTABLE)
	{
		csp_print(" -R <rtable>      set routing table\n");
	}
	if (1)
	{
		csp_print(" -a <address>     set interface address\n"
				  " -C <address>     connect to server at address\n"
				  " -f <file src>	 source of file to be sent\n"
				  " -t               enable test mode\n"
				  " -T <duration>    enable test mode with running time in seconds\n"
				  " -h               print help\n");
	}
}

csp_iface_t *add_interface(enum DeviceType device_type, const char *device_name)
{
	csp_iface_t *default_iface = NULL;

	if (device_type == DEVICE_KISS)
	{
		csp_usart_conf_t conf = {
			.device = device_name,
			.baudrate = 115200, /* supported on all platforms */
			.databits = 8,
			.stopbits = 1,
			.paritysetting = 0,
		};
		int error = csp_usart_open_and_add_kiss_interface(&conf, CSP_IF_KISS_DEFAULT_NAME, &default_iface);
		if (error != CSP_ERR_NONE)
		{
			csp_print("failed to add KISS interface [%s], error: %d\n", device_name, error);
			exit(1);
		}
		default_iface->is_default = 1;
	}

	if (CSP_HAVE_LIBSOCKETCAN && (device_type == DEVICE_CAN))
	{
		int error = csp_can_socketcan_open_and_add_interface(device_name, CSP_IF_CAN_DEFAULT_NAME, client_address, 1000000, true, &default_iface);
		if (error != CSP_ERR_NONE)
		{
			csp_print("failed to add CAN interface [%s], error: %d\n", device_name, error);
			exit(1);
		}
		default_iface->is_default = 1;
	}

	if (CSP_HAVE_LIBZMQ && (device_type == DEVICE_ZMQ))
	{
		int error = csp_zmqhub_init(client_address, device_name, 0, &default_iface);
		if (error != CSP_ERR_NONE)
		{
			csp_print("failed to add ZMQ interface [%s], error: %d\n", device_name, error);
			exit(1);
		}
		default_iface->is_default = 1;
	}

	return default_iface;
}

/* main - initialization of CSP and start of client task */
int main(int argc, char *argv[])
{

	const char *device_name = NULL;
	enum DeviceType device_type = DEVICE_UNKNOWN;
	const char *rtable __maybe_unused = NULL;
	csp_iface_t *default_iface;
	struct timespec start_time;
	unsigned int count;
	int ret = EXIT_SUCCESS;
	int opt;

	while ((opt = getopt_long(argc, argv, OPTION_c OPTION_z OPTION_R "k:a:C:f:tT:h", long_options, NULL)) != -1)
	{
		switch (opt)
		{
		case 'c':
			device_name = optarg;
			device_type = DEVICE_CAN;
			break;
		case 'k':
			device_name = optarg;
			device_type = DEVICE_KISS;
			break;
		case 'z':
			device_name = optarg;
			device_type = DEVICE_ZMQ;
			break;
		case 'f':
			file_src = optarg;
			break;
#if (CSP_USE_RTABLE)
		case 'R':
			rtable = optarg;
			break;
#endif
		case 'a':
			client_address = atoi(optarg);
			break;
		case 'C':
			server_address = atoi(optarg);
			break;
		case 't':
			test_mode = true;
			break;
		case 'T':
			test_mode = true;
			run_duration_in_sec = atoi(optarg);
			break;
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
		case '?':
			// Invalid option or missing argument
			print_help();
			exit(EXIT_FAILURE);
		}
	}

	// Unless one of the interfaces are set, print a message and exit
	if (device_type == DEVICE_UNKNOWN)
	{
		csp_print("One and only one of the interfaces can be set.\n");
		print_help();
		exit(EXIT_FAILURE);
	}

	csp_print("Initialising CSP\n");

	/* Init CSP */
	csp_init();

	/* Start router */
	router_start();

	/* Add interface(s) */
	default_iface = add_interface(device_type, device_name);

	/* Setup routing table */
	if (CSP_USE_RTABLE)
	{
		if (rtable)
		{
			int error = csp_rtable_load(rtable);
			if (error < 1)
			{
				csp_print("csp_rtable_load(%s) failed, error: %d\n", rtable, error);
				exit(1);
			}
		}
		else if (default_iface)
		{
			csp_rtable_set(0, 0, default_iface, CSP_NO_VIA_ADDRESS);
		}
	}

	csp_print("Connection table\r\n");
	csp_conn_print_table();

	csp_print("Interfaces\r\n");
	csp_iflist_print();

	if (CSP_USE_RTABLE)
	{
		csp_print("Route table\r\n");
		csp_rtable_print();
	}

	/* Start client work */
	csp_print("Client started\n");

	static csp_socket_t sock = {0};
	sock.opts = CSP_O_RDP;
	csp_bind(&sock, PORT);
	csp_listen(&sock, 1); // This allows only one simultaneous connection

	csp_conn_t *conn;

	while (1)
	{
		if ((conn = csp_accept(&sock, 10000)) == NULL)
		{
			continue;
		}

		csp_packet_t *request = csp_read(conn, 50);

		if (request->data[0] == UPLOAD_CLIENT_DTP_UPLOAD_REQUEST)
		{
			if (request->length < 5)
			{
				csp_print("Invalid DTP upload request: too short\n");
			}
			else
			{
				uint8_t dtp_server_addr = request->data[1];
				uint16_t payload_id;
				memcpy(&payload_id, &request->data[2], sizeof(uint16_t));
				char *file_location = (char *)&request->data[4];

				csp_print("DTP upload request: server %u, payload %u, file '%s'\n", dtp_server_addr, payload_id, file_location);

				FILE *output_file = fopen(file_location, "wb");

				if (output_file == NULL)
				{
					csp_print("Error: Could not create file '%s'. Errno: %d\n", file_location, errno);
					csp_packet_t *response = csp_buffer_get(1);
					if (response)
					{
						response->length = 1;
						response->data[0] = 0; // 0 for failure
						csp_send(conn, response);
						csp_buffer_free(response);
					}
				}
				else
				{
					csp_print("File '%s' created. Starting transfer.\n", file_location);
					csp_packet_t *response = csp_buffer_get(1);
					if (response)
					{
						response->length = 1;
						response->data[0] = 1; // 1 for success
						csp_send(conn, response);
						csp_buffer_free(response);
					}

					dtp_thread_args_t *thread_args = malloc(sizeof(dtp_thread_args_t));
					if (thread_args == NULL)
					{
						csp_print("Failed to allocate memory for thread args\n");
						fclose(output_file);
					}
					else
					{
						thread_args->server_addr = dtp_server_addr;
						thread_args->payload_id = payload_id;
						thread_args->output_file = output_file;

						pthread_t dtp_thread;
						if (pthread_create(&dtp_thread, NULL, dtp_client_worker, thread_args) != 0)
						{
							csp_print("Failed to start DTP client thread\n");
							fclose(output_file);
							free(thread_args);
						}
						else
						{
							pthread_detach(dtp_thread); // Run thread in the background
						}
					}
				}

				/*
				if (file = attempt_create_file(request->file_location))
				{
					// File created succesfully
					file_to_write_to = file; // Persist the name of the file which the current upload is writing to
					// Run dtp_client_main, see: https://github.com/spaceinventor/libdtp/blob/master/include/dtp/dtp.h
					// Similar to how its done in the dipp apm: https://github.com/discosat/dipp-apm/blob/main/src/dtp_client_apm.c
					dtp_client_main(dtp_server_addr, payload_id, ...);
				}
				else
				{
					// Couldn't create file
					response = csp_buffer_get;
					response->data = file location invalid;
					csp_send(response);
				}
					*/
			}

			usleep(100000);
		}

		/* Wait for execution to end (ctrl+c) */

		return ret;
	}
}