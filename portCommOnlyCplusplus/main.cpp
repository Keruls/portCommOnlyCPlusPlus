#include "head.h"
#include "MyPort.h"
int main() {
	MyPort m;
	//int kb_value;
	if (m.port_open("\\\\.\\COM2",9600,0,0,8))//异步
	{
		cout << "port open success!" << endl;
		string s;
		cout << "input and send:" << endl; 
		getline(cin,s);
		m.send(s);
		//方式二
		if (m.thread_open()) {
			cout << "thread open success!" << endl;
		}
		//不阻塞主线程的情况下子线程和主线程会同时进行，当主线程结束时子线程也会被动结束,
		// 阻塞主线程，等待子线程信号或100s后再继续运行主线程
		WaitForSingleObject(m.thread_signal, 100000);
		
		//方式一
		//while (1)
		//{
		//	//程序会一直处于读的状态，每次读的间隔时间为串口总超时的时间
		//	cout << m.receive();
		//}
	}
	else cout << "port open failed!" << endl;

	//system("pause");
	return 0;
}