// Based on https://github.com/dormon/3DApps/blob/master/src/holoCalibration.cpp

#include "UsbCalibration.h"
#include <libusb-1.0/libusb.h>
#include <stdexcept>
#include <iostream>
#include <nlohmann/json.hpp>

std::vector<HoloDevice> UsbCalibration::getDevices()
{
	libusb_context* c;
	if (libusb_init(&c) < 0)
		throw std::runtime_error("Cannot Initialize libusb");

	libusb_device** devs;
	ssize_t devCount = libusb_get_device_list(c, &devs);

	if (devCount <= 0)
		throw std::runtime_error("No devices listed");

	int productId{ 0 };
	for (int i = 0; i < devCount; i++)
	{
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(devs[i], &desc) < 0)
			continue;
		if (desc.idVendor == VENDOR_ID)
		{
			productId = desc.idProduct;
			break;
		}
	}

	if (productId == 0)
		throw std::runtime_error("No LKG display found");

	return { HoloDevice{productId} };
}

Calibration UsbCalibration::getCalibration(HoloDevice device)
{
	struct libusb_device_handle* dh = NULL;
	dh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, device.productId);

	if (!dh)
		throw std::runtime_error("Cannot connect to device");

#if WINDOWS
	libusb_detach_kernel_driver(dh, INTERFACE_NUM);
#endif
	bool foundInterf = false;
	for (int interf = MIN_INTERFACE_NUM; interf <= MAX_INTERFACE_NUM; interf++)
	{
		std::cout << "Trying interface " << interf << std::endl;
		auto claimed = libusb_claim_interface(dh, interf);
		if (claimed >= 0)
		{
			std::cout << "Connecting to interface " << interf << "..." << std::endl;
			foundInterf = true;
			break;
		}
		else
		{
			std::cerr << "USB error: " << libusb_strerror(claimed) << std::endl;
		}
	}
	if (!foundInterf)
	{
		throw std::runtime_error("Cannot claim any interface");
	}

	uint8_t bmReqType = 0x80;
	uint8_t bReq = 6;
	uint16_t wVal = 0x0302;
	uint16_t wIndex = 0x0409;
	unsigned char data[255] = { 0 };
	std::vector<unsigned char> buffer;
	unsigned int to = 0;
	libusb_control_transfer(dh, bmReqType, bReq, 0x0303, wIndex, data, 1026, to);
	libusb_control_transfer(dh, bmReqType, bReq, 0x0301, wIndex, data, 1026, to);
	libusb_control_transfer(dh, bmReqType, bReq, wVal, wIndex, data, 255, to);

	buffer.resize(BUFFER_SIZE);
	std::fill(buffer.begin(), buffer.end(), 0);

	int len = 0;
	std::string info;
	for (int i = 0; i < PACKET_NUM; i++)
	{
		int r;
		std::fill(buffer.begin(), buffer.end(), 0);
		buffer[2] = i;
		r = libusb_control_transfer(dh, 0x21, 9, 0x300, 0x02, buffer.data(), INTERRUPT_SIZE, to);
		if (r < 0)
			throw std::runtime_error("Cannot send control");

		std::fill(buffer.begin(), buffer.end(), 0);
		r = libusb_interrupt_transfer(dh, 0x84, buffer.data(), buffer.size(), &len, 0);
		info.insert(info.end(), buffer.begin(), buffer.begin() + INTERRUPT_SIZE + 1);
		if (r < 0)
			throw std::runtime_error("Cannot interrupt");
	}
	info = info.substr(info.find("{"), info.find("}}") - info.find("{") + 2);

	libusb_close(dh);
	libusb_exit(NULL);

	for (int i = 0; i < 7; i++)
		info.erase(remove(info.begin(), info.end(), i), info.end());
	auto j = nlohmann::json::parse(info.c_str());

	return jsonToCaibration(j);
}