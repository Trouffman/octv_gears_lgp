#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>

#define CAMERA_VENDOR				0x07ca
#define CAMERA_PRODUCT				0x0875	
#define CAMERA_CONFIGURATION		1
#define CAMERA_INTERACE				0

#define CAMERA_ENDPOINT_ADDRESS_VIDEO_CAPTURE		0x81
#define CAMERA_ENDPOINT_ADDRESS_CONTROL				0x04
#define CAMERA_ENDPOINT_ADDRESS_STATUS_RESPONSE		0x83
#define TIMEOUT	5000

int writecommand(libusb_device_handle *camerahandle, char* commandbuffer, size_t size) {
	static int transferred = 0;
	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_CONTROL, commandbuffer, size, &transferred, TIMEOUT);
	if(err != 0) {
		printf("Error while sending command: '%s' - '%s',  data sent: %i, data transferred: %i, crashing!\n", libusb_error_name(err), libusb_strerror(err), 512, transferred);
		exit(-5);
	}
}

int readstatus(libusb_device_handle *camerahandle) {
	static char buffer[512];
	memset(buffer, 0, 512);
	static int transferred = 0;


	int err = libusb_bulk_transfer(camerahandle, CAMERA_ENDPOINT_ADDRESS_STATUS_RESPONSE, buffer, 512, &transferred, TIMEOUT);

	if(err != 0) {
		printf("Error while reading command: '%s' - '%s' , data received: %i, crashing!\n", libusb_error_name(err), libusb_strerror(err),transferred);
		exit(-5);
	}


	printf("Status received, bytes %i!, value: %i \n", transferred, buffer[0]);
}

int main(int argc, char **argv) {
	// Process:
	// 1. Grab USB context
	// 2. Query system devices
	// 3. Figure out which one is the camera
	// 4. Configure the camera
	// 5. Claim interfaces
	// 6. Comm
	// 7. Cleanup


	// 1. Grab USB context
	libusb_context *usbcontext = NULL;
	if(libusb_init(&usbcontext) == 0)
		printf("We got the context\n");
	else {
		printf("We DONT have the context\n");
		exit(1);
	}

	libusb_set_debug(usbcontext, LIBUSB_LOG_LEVEL_WARNING);

	
	// 2. Query system devices
	libusb_device **devicelist;
	size_t devicecount = libusb_get_device_list(usbcontext, &devicelist);
	if(devicecount < 0) {
		printf("Error when counting devices!\n");
		exit(-1);
	}

	// 3. Figure out which one is the camera
	libusb_device_handle *camerahandle = NULL;
	for(size_t i = 0; i < devicecount; i++) {
		libusb_device *dev = devicelist[i];	
		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor(dev, &desc);

		if(desc.idVendor == CAMERA_VENDOR && desc.idProduct == CAMERA_PRODUCT) {
			printf("Found camera!\n");
			if(libusb_open(dev, &camerahandle) != 0) {
				printf("Error getting the handle!\n");
				camerahandle = NULL;
			}
		}
	}


	if(camerahandle == NULL) {
		printf("Couldn't obtain camera handle.\n");
		exit(-2);
	}


	// 4. Configure the camera
	if(libusb_set_configuration(camerahandle, CAMERA_CONFIGURATION) != 0) {
		printf("Failed to set configuration!\n");
		exit(-3);
	}


	// 5. Claim interfaces
	if(libusb_claim_interface(camerahandle, CAMERA_INTERACE) != 0) {
		printf("Failed to claim interface!\n");
		exit(-4);
	}

	// 6. Comm

	char id_00100[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x0b  };
	size_t id_00100_s = 10;	
	char id_00101[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x03  };
	size_t id_00101_s = 10;	
	char id_00102[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x05  };
	size_t id_00102_s = 10;
	char id_00103[] = { 0x01, 0x01, 0x01, 0x00, 0xF8, 0x06, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00  };
	size_t id_00103_s = 12;
	char id_00104[] = { 0x01, 0x01, 0x01, 0x00, 0xCC, 0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00  };
	size_t id_00104_s = 12;
	char id_00105[] = { 0x01, 0x01, 0x01, 0x00, 0xFC, 0x06, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00  };
	size_t id_00105_s = 12;
	char id_00106[] = { 0x0B, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x3A, 0x03  };
	size_t id_00106_s = 10;
 	char id_00107[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2B  };
	size_t id_00107_s = 9;
	char id_00108[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x3E  };
	size_t id_00108_s = 9;
 	char id_00109[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x1B  };
	size_t id_00109_s = 9;
 	char id_00110[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x1C  };
	size_t id_00110_s = 9;
 	char id_00111[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x25  };
	size_t id_00111_s = 9;
 	char id_00112[] = { 0x0C, 0x01, 0x07, 0x00, 0x15, 0x00, 0x00, 0x00, 0x1D  };
	size_t id_00112_s = 9;
 	char id_00113[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x37  };
	size_t id_00113_s = 9;
 	char id_00114[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2D  };
	size_t id_00114_s = 9;
 	char id_00115[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x26  };
	size_t id_00115_s = 9;
 	char id_00116[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x24  };
	size_t id_00116_s = 9;
 	char id_00117[] = { 0x0C, 0x01, 0x01, 0x00, 0x15, 0x00, 0x00, 0x00, 0x13  };
	size_t id_00117_s = 9;




	// Boot
	printf("Init procedure...\n");
	writecommand(camerahandle, id_00100, id_00100_s);
	readstatus(camerahandle);
	sleep(5);

	writecommand(camerahandle, id_00101, id_00101_s);
	readstatus(camerahandle);
	sleep(5);

	writecommand(camerahandle, id_00102, id_00102_s);
	readstatus(camerahandle);
	sleep(5);
	printf("Done\n");



	// Something	
	printf("Something...\n");
	char i_00200[] = { 0x0b, 0x01, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x2c, 0x0b  };
	size_t id_00200_s = 10;	



	printf("Done\n");



	// 7. Cleanup
	printf("Closing handles...\n");
	libusb_release_interface(camerahandle, CAMERA_INTERACE);
	libusb_close(camerahandle);
	libusb_free_device_list(devicelist, 1); // 1 = unref devices
	libusb_exit(usbcontext);
	printf("Bye\n");
	return 0;
}
