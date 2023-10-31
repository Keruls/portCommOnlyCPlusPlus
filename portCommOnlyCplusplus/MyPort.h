#pragma once
#include <string>
#include <iostream>
#include <WinSock2.h>
#include <windows.h>
using namespace std;
class MyPort
{
public:
	MyPort();
	~MyPort();
	//端口打开
	bool port_open(const char* portname,
		int baudrate = 9600,
		char parity = NOPARITY,
		char stop = ONESTOPBIT,
		char data = DATABITS_8);
	//端口关闭
	void port_close();
	//发送
	bool send(string send_data);
	//读取
	string receive();
	//线程退出信号SetEvent(thread_signal)
	HANDLE thread_signal;
	//创建线程
	bool thread_open();
	//监听线程
	static DWORD WINAPI thread_listen(LPVOID param);
	//关闭线程
	bool thread_close();
	//获取当前缓存区字节数
	int get_BufferBytes(string mode);
	//解析接收到的数据
	void analysis(const char* buf, int readbyte);

private:
	//int h_port[32];//HANDLE为32位无符号整数值
	HANDLE port_h;//端口句柄
	
	HANDLE readThread_h;// 读线程句柄
	HANDLE writeThread_h;// 写线程句柄
};

