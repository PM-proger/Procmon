#pragma once

struct WindowsParams_RECV_STRUCT {
	std::string ServerIP;
	std::string ServerPage;
	HINSTANCE hInst = NULL;
	HWND hMainWnd = NULL;
};

void ReceiveDataFromServer(const WindowsParams_RECV_STRUCT& w);