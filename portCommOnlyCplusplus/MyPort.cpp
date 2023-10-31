#include "MyPort.h"

MyPort::MyPort()
{
	port_h = INVALID_HANDLE_VALUE;
	readThread_h = INVALID_HANDLE_VALUE;
	writeThread_h = INVALID_HANDLE_VALUE;
	thread_signal = INVALID_HANDLE_VALUE;
	//创建事件，用于向主线程发送通知，解除主线程阻塞
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
	//打开端口
	port_h = CreateFileA(portname, //串口名
		GENERIC_READ | GENERIC_WRITE, //支持读写
		0, //独占方式，串口不支持共享
		NULL,//安全属性指针，默认值为NULL
		OPEN_EXISTING, //打开现有的串口文件
		FILE_FLAG_OVERLAPPED, //0：同步方式，FILE_FLAG_OVERLAPPED：异步方式
		NULL);//用于复制文件句柄，默认值为NULL，对串口而言该参数必须置为NULL
	//设置输入/输出缓冲区大小
	if (!SetupComm(port_h, 1024, 1024))
	{
		cout << "set buffer failed!" << endl;
		return false;
	};
	//创建DCB结构体，要修改串口的配置，应该先修改 DCB 结构，其包含了串口的各项参数设置
	DCB d;
	memset(&d, 0, sizeof(d));
	d.DCBlength = sizeof(d);
	d.BaudRate = baudrate;
	d.StopBits = stop;
	d.Parity = parity;
	d.ByteSize = data;
	//设置串口
	if (!SetCommState(port_h, &d))
	{
		cout << "set comm state failed!" << endl;
		return false;
	}
	//设置超时单位ms
	// 总超时＝时间系数×读或写的字符数＋时间常量
	COMMTIMEOUTS time;
	time.ReadIntervalTimeout = 1000;//读间隔超时，读取每个字符的最大时间间隔，写没有间隔超时
	time.ReadTotalTimeoutConstant = 5000;//读时间常量
	time.ReadTotalTimeoutMultiplier = 10;//读时间系数
	time.WriteTotalTimeoutConstant = 5000;//写时间常量
	time.WriteTotalTimeoutMultiplier = 100;//写时间系数
	SetCommTimeouts(port_h, &time);

	//清理缓冲区
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

//读取方式二
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
	//获取本类对象，源对象来自CreateThread函数传递的this指针
	MyPort* myPort = static_cast<MyPort*> (param);

	//存储读取的字节
	char buf[1024] = {};

	//待输出字符串
	string receive_data = "";

	/*重叠（异步）操作所用的结构体，通过结构体成员hEvent判断overlapped操作是否完成
	* 此处用于配合线程阻塞函数waitForSignalObject(HANDLE name,)使用，或getOverLappedResult(handle, lpOverlapped, lpNumberOfBytesTransferred, bWait)
	* 当串口读操作完成时，会将成员hEvent置位，发出信号，此时线程将不再阻塞
	* 也可通过setEvent(HANDLE name)主动发出信号
	*/
	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(OVERLAPPED));
	

	//创建一个用于OVERLAPPED的事件处理，不会真正用到？但系统要求这么做？
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	//轮询
	while (1) {
		
		//获取当前缓存区字节数
		int readBytes = myPort->get_BufferBytes("getIn");
		
		//若缓存区无内容则挂起等待
		if (readBytes == 0) {
			//挂起时间尽可能短，否则发送端数据发送时间间隔小于挂起时间时，接收端会将对方多次发送的数据当成一次发送的数据，黏包？
			Sleep(10);
			continue;
		}
		cout << "\n";
		//读取输入缓冲区数据，放入buf字符数组中,readBytes
		bool readstate = ReadFile(myPort->port_h, buf, readBytes, NULL, &overlapped);
		
		/*
		* 当readBytes比较大时，读取花费的时间较长, 此时函数会持续往下执行，若函数执行完毕前一刻还未读取完则不会有任何输出
		* 这种情况下我们需要施加阻塞以等待读取完成，然后再继续执行
		*/
		//若正在读取则等待2s，其他错误直接退出
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
		//解析数据
		myPort->analysis(buf, readBytes);
		
		//不发送事件通知则持续监听串口
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
	COMSTAT comStat;//通讯状态
	DWORD errorFlag = 0;//错误标识
	memset(&comStat, 0, sizeof(comStat));
	//获取端口当前状态
	ClearCommError(port_h, &errorFlag, &comStat);
	if (mode == "getIn") return comStat.cbInQue;
	else if (mode == "getOut") return comStat.cbOutQue;
	cout << "get_InBufferBytes mode not exist!" << endl;
	return 0;
}

void MyPort::analysis(const char* buf, int readbyte)
{
	string receive_data = "";
	//字符数组组合成字符串
	for (int i = 0; i < readbyte; i++)
	{
		if (buf[i] != -52)
			receive_data += buf[i];
		else
			break;
	}
	//接收到退出命令，关闭监听线程，发送信号通知
	if (receive_data == "exit") { 
		cout << "get exit order!" << endl;
		thread_close(); 
		SetEvent(thread_signal); 
	}
	//其他情况直接输出数据
	else cout << receive_data << endl;

}

//读取方式一
string MyPort::receive()
{
	char buf[1024];
	int inbuf = 0;
	string receive_data;
	DWORD got_byte;
	DWORD dwErrorFlags;//错误标志
	COMSTAT comStat;//通讯状态
	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(overlapped));
	//创建一个用于OVERLAPPED的事件处理，不会真正用到，但系统要求这么做
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//获取端口当前状态
	ClearCommError(port_h, &dwErrorFlags, &comStat);
	inbuf = get_BufferBytes("getIn");
	//若输入缓冲区为0，返回‘’
	if (inbuf == 0) { Sleep(10); return ""; }
	//读取输入缓冲区数据，放入buf字符数组中
	bool readstate = ReadFile(port_h, buf, inbuf, &got_byte, &overlapped);
	if (!readstate)
	{
		//重叠I / O非常灵活，它也可以实现阻塞（例如我们可以设置一定要读取到一个数据才能进行到下一步操作）。
		// 有两种方法可以等待操作完成：
		// 一种方法是用象WaitForSingleObject这样的等待函数来等待OVERLAPPED结构的hEvent成员；
		// 另一种方法是调用GetOverlappedResult函数等待
		if (GetLastError() == ERROR_IO_PENDING) //如果串口正在读取中
		{
			//下面两条语句都可使用
			//GetOverlappedResult函数的最后一个参数设为TRUE
			//bWait为TRUE时函数会一直等待，直到读操作完成或由于错误而返回
			GetOverlappedResult(port_h, &overlapped, &got_byte, TRUE);

			//使用WaitForSingleObject函数等待，直到读操作完成或延时已达到2秒钟
			//当串口读操作进行完毕后，overlapped的hEvent事件会变为有信号
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

