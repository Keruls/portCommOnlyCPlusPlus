#include "head.h"
#include "MyPort.h"
int main() {
	MyPort m;
	//int kb_value;
	if (m.port_open("\\\\.\\COM2",9600,0,0,8))//�첽
	{
		cout << "port open success!" << endl;
		string s;
		cout << "input and send:" << endl; 
		getline(cin,s);
		m.send(s);
		//��ʽ��
		if (m.thread_open()) {
			cout << "thread open success!" << endl;
		}
		//���������̵߳���������̺߳����̻߳�ͬʱ���У������߳̽���ʱ���߳�Ҳ�ᱻ������,
		// �������̣߳��ȴ����߳��źŻ�100s���ټ����������߳�
		WaitForSingleObject(m.thread_signal, 100000);
		
		//��ʽһ
		//while (1)
		//{
		//	//�����һֱ���ڶ���״̬��ÿ�ζ��ļ��ʱ��Ϊ�����ܳ�ʱ��ʱ��
		//	cout << m.receive();
		//}
	}
	else cout << "port open failed!" << endl;

	//system("pause");
	return 0;
}