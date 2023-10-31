#include "MyPort.h"

MyPort::MyPort()
{
	port_h = INVALID_HANDLE_VALUE;
	readThread_h = INVALID_HANDLE_VALUE;
	writeThread_h = INVALID_HANDLE_VALUE;
	thread_signal = INVALID_HANDLE_VALUE;
	//�����¼������������̷߳���֪ͨ��������߳�����
	thread_signal = CreateEvent(NULL, TRUE, FALSE, L"finish_thread");
}

MyPort::~MyPort()
{
	
	thread_close();
	port_close();
}

bool MyPort::port_open(const char* portname,
	int baudrate,
	char parity,
	char stop,
	char data)
{
	//�򿪶˿�
	port_h = CreateFileA(portname, //������
		GENERIC_READ | GENERIC_WRITE, //֧�ֶ�д
		0, //��ռ��ʽ�����ڲ�֧�ֹ���
		NULL,//��ȫ����ָ�룬Ĭ��ֵΪNULL
		OPEN_EXISTING, //�����еĴ����ļ�
		FILE_FLAG_OVERLAPPED, //0��ͬ����ʽ��FILE_FLAG_OVERLAPPED���첽��ʽ
		NULL);//���ڸ����ļ������Ĭ��ֵΪNULL���Դ��ڶ��Ըò���������ΪNULL
	//��������/�����������С
	if (!SetupComm(port_h, 1024, 1024))
	{
		cout << "set buffer failed!" << endl;
		return false;
	};
	//����DCB�ṹ�壬Ҫ�޸Ĵ��ڵ����ã�Ӧ�����޸� DCB �ṹ��������˴��ڵĸ����������
	DCB d;
	memset(&d, 0, sizeof(d));
	d.DCBlength = sizeof(d);
	d.BaudRate = baudrate;
	d.StopBits = stop;
	d.Parity = parity;
	d.ByteSize = data;
	//���ô���
	if (!SetCommState(port_h, &d))
	{
		cout << "set comm state failed!" << endl;
		return false;
	}
	//���ó�ʱ��λms
	// �ܳ�ʱ��ʱ��ϵ��������д���ַ�����ʱ�䳣��
	COMMTIMEOUTS time;
	time.ReadIntervalTimeout = 1000;//�������ʱ����ȡÿ���ַ������ʱ������дû�м����ʱ
	time.ReadTotalTimeoutConstant = 5000;//��ʱ�䳣��
	time.ReadTotalTimeoutMultiplier = 10;//��ʱ��ϵ��
	time.WriteTotalTimeoutConstant = 5000;//дʱ�䳣��
	time.WriteTotalTimeoutMultiplier = 100;//дʱ��ϵ��
	SetCommTimeouts(port_h, &time);

	//��������
	PurgeComm(port_h, PURGE_RXCLEAR || PURGE_TXCLEAR);

	return true;
}

void MyPort::port_close()
{
	CloseHandle(port_h);
	cout << "close port!" << endl;
}

bool MyPort::send(string send_data)
{
	OVERLAPPED olapped;
	DWORD sendOutByte = 0;
	memset(&olapped, 0, sizeof(OVERLAPPED));
	olapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	DWORD obufbyte = send_data.size();//get_InBufferBytes("getOut");
	bool writeState = WriteFile(port_h, (char*)send_data.c_str(), obufbyte, &sendOutByte, &olapped);
	if (!writeState) {
		DWORD error = GetLastError();
		if (error == ERROR_IO_PENDING) {
			cout << "WRITE_PENDING" << endl;
			WaitForSingleObject(olapped.hEvent, 2000);
		}
		else {
			cout << error << ",send failure!" << endl;
			return false;
		}
	}
	return true;
}

//��ȡ��ʽ��
bool MyPort::thread_open()
{
	if (readThread_h != INVALID_HANDLE_VALUE) {
		cout << "thread had opened!" << endl;
		return false;
	}
	//DWORD thread_id;
	readThread_h = CreateThread(NULL, 0, thread_listen, this, 0, NULL);

	if (!readThread_h) {
		cout << "open thread filed!" << endl;
		return false;
	}
	//SetThreadPriority(thread_h, THREAD_PRIORITY_ABOVE_NORMAL);
	return true;
}

DWORD WINAPI MyPort::thread_listen(LPVOID param)
{
	//��ȡ�������Դ��������CreateThread�������ݵ�thisָ��
	MyPort* myPort = static_cast<MyPort*> (param);

	//�洢��ȡ���ֽ�
	char buf[1024] = {};

	//������ַ���
	string receive_data = "";

	/*�ص����첽���������õĽṹ�壬ͨ���ṹ���ԱhEvent�ж�overlapped�����Ƿ����
	* �˴���������߳���������waitForSignalObject(HANDLE name,)ʹ�ã���getOverLappedResult(handle, lpOverlapped, lpNumberOfBytesTransferred, bWait)
	* �����ڶ��������ʱ���Ὣ��ԱhEvent��λ�������źţ���ʱ�߳̽���������
	* Ҳ��ͨ��setEvent(HANDLE name)���������ź�
	*/
	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(OVERLAPPED));
	

	//����һ������OVERLAPPED���¼��������������õ�����ϵͳҪ����ô����
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	//��ѯ
	while (1) {
		
		//��ȡ��ǰ�������ֽ���
		int readBytes = myPort->get_BufferBytes("getIn");
		
		//�������������������ȴ�
		if (readBytes == 0) {
			//����ʱ�価���̣ܶ������Ͷ����ݷ���ʱ����С�ڹ���ʱ��ʱ�����ն˻Ὣ�Է���η��͵����ݵ���һ�η��͵����ݣ�����
			Sleep(10);
			continue;
		}
		cout << "\n";
		//��ȡ���뻺�������ݣ�����buf�ַ�������,readBytes
		bool readstate = ReadFile(myPort->port_h, buf, readBytes, NULL, &overlapped);
		
		/*
		* ��readBytes�Ƚϴ�ʱ����ȡ���ѵ�ʱ��ϳ�, ��ʱ�������������ִ�У�������ִ�����ǰһ�̻�δ��ȡ���򲻻����κ����
		* ���������������Ҫʩ�������Եȴ���ȡ��ɣ�Ȼ���ټ���ִ��
		*/
		//�����ڶ�ȡ��ȴ�2s����������ֱ���˳�
		if (!readstate) {
			DWORD error = GetLastError();
			if (error == ERROR_IO_PENDING) {
				cout << "READ_PENDING" << endl;
				WaitForSingleObject(overlapped.hEvent, 2000);
			}
			else {
				cout << error << ",read failure!" << endl;
				return false;
			}
		}
		//��������
		myPort->analysis(buf, readBytes);
		
		//�������¼�֪ͨ�������������
		//SetEvent(myPort->thread_signal);
	}
	cout << "listenthread shutdown!" << endl;
	return true;
}

bool MyPort::thread_close()
{
	if (readThread_h != INVALID_HANDLE_VALUE) {
		Sleep(100);
		CloseHandle(readThread_h);
		readThread_h = INVALID_HANDLE_VALUE;
		cout << "close thread!" << endl;
	}
	return true;
}

int MyPort::get_BufferBytes(string mode)
{
	COMSTAT comStat;//ͨѶ״̬
	DWORD errorFlag = 0;//�����ʶ
	memset(&comStat, 0, sizeof(comStat));
	//��ȡ�˿ڵ�ǰ״̬
	ClearCommError(port_h, &errorFlag, &comStat);
	if (mode == "getIn") return comStat.cbInQue;
	else if (mode == "getOut") return comStat.cbOutQue;
	cout << "get_InBufferBytes mode not exist!" << endl;
	return 0;
}

void MyPort::analysis(const char* buf, int readbyte)
{
	string receive_data = "";
	//�ַ�������ϳ��ַ���
	for (int i = 0; i < readbyte; i++)
	{
		if (buf[i] != -52)
			receive_data += buf[i];
		else
			break;
	}
	//���յ��˳�����رռ����̣߳������ź�֪ͨ
	if (receive_data == "exit") { 
		cout << "get exit order!" << endl;
		thread_close(); 
		SetEvent(thread_signal); 
	}
	//�������ֱ���������
	else cout << receive_data << endl;

}

//��ȡ��ʽһ
string MyPort::receive()
{
	char buf[1024];
	int inbuf = 0;
	string receive_data;
	DWORD got_byte;
	DWORD dwErrorFlags;//�����־
	COMSTAT comStat;//ͨѶ״̬
	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(overlapped));
	//����һ������OVERLAPPED���¼��������������õ�����ϵͳҪ����ô��
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//��ȡ�˿ڵ�ǰ״̬
	ClearCommError(port_h, &dwErrorFlags, &comStat);
	inbuf = get_BufferBytes("getIn");
	//�����뻺����Ϊ0�����ء���
	if (inbuf == 0) { Sleep(10); return ""; }
	//��ȡ���뻺�������ݣ�����buf�ַ�������
	bool readstate = ReadFile(port_h, buf, inbuf, &got_byte, &overlapped);
	if (!readstate)
	{
		//�ص�I / O�ǳ�����Ҳ����ʵ���������������ǿ�������һ��Ҫ��ȡ��һ�����ݲ��ܽ��е���һ����������
		// �����ַ������Եȴ�������ɣ�
		// һ�ַ���������WaitForSingleObject�����ĵȴ��������ȴ�OVERLAPPED�ṹ��hEvent��Ա��
		// ��һ�ַ����ǵ���GetOverlappedResult�����ȴ�
		if (GetLastError() == ERROR_IO_PENDING) //����������ڶ�ȡ��
		{
			//����������䶼��ʹ��
			//GetOverlappedResult���������һ��������ΪTRUE
			//bWaitΪTRUEʱ������һֱ�ȴ���ֱ����������ɻ����ڴ��������
			GetOverlappedResult(port_h, &overlapped, &got_byte, TRUE);

			//ʹ��WaitForSingleObject�����ȴ���ֱ����������ɻ���ʱ�Ѵﵽ2����
			//�����ڶ�����������Ϻ�overlapped��hEvent�¼����Ϊ���ź�
			//WaitForSingleObject(overlapped.hEvent, 2000);
			//cout << "do nothing." << endl;
		}
	}
	for (int i = 0; i < inbuf; i++)
	{
		if (buf[i] != -52)
			receive_data += buf[i];
		else
			break;
	}
	cout << "\n-------------------read finish!-------------------" << endl;
	return receive_data + '\n';
}

