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
	//�˿ڴ�
	bool port_open(const char* portname,
		int baudrate = 9600,
		char parity = NOPARITY,
		char stop = ONESTOPBIT,
		char data = DATABITS_8);
	//�˿ڹر�
	void port_close();
	//����
	bool send(string send_data);
	//��ȡ
	string receive();
	//�߳��˳��ź�SetEvent(thread_signal)
	HANDLE thread_signal;
	//�����߳�
	bool thread_open();
	//�����߳�
	static DWORD WINAPI thread_listen(LPVOID param);
	//�ر��߳�
	bool thread_close();
	//��ȡ��ǰ�������ֽ���
	int get_BufferBytes(string mode);
	//�������յ�������
	void analysis(const char* buf, int readbyte);

private:
	//int h_port[32];//HANDLEΪ32λ�޷�������ֵ
	HANDLE port_h;//�˿ھ��
	
	HANDLE readThread_h;// ���߳̾��
	HANDLE writeThread_h;// д�߳̾��
};

