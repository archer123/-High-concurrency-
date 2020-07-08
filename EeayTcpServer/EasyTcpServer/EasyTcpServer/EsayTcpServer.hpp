#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS

	#include <WinSock2.h>
	#include <windows.h>
	#pragma comment(lib,"ws2_32.lib")       //��������  ������ͨ�õķ��� �Ѿ���������������
#else
	#include <unistd.h>             //unix�ı�׼��
	#include <arpa/inet.h>
	#include <string.h>

	#define SOCKET int
	#define INVALID_SOCKET (SOCKET)(~0)
	#define SOCKET_ERROR (-1)
#endif

#include <iostream>
#include <vector>
#include "CELLTimestamp.hpp"
#include "MessageHeader.hpp"
using namespace std;
#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240
#endif // !RECV_BUFF_SIZE

class ClientSocket
{
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET) {
		_sockfd = sockfd;
		_lastpos = 0;
		memset(_szMsgBuf,0,sizeof(_szMsgBuf));
		
	}

	SOCKET sockfd()
	{
		return _sockfd;
	}

	char* msgBuf()
	{
		return _szMsgBuf;
	}

	int getlast()
	{
		return _lastpos;
	}

	void setLast(int pos)
	{
		_lastpos = pos;
	}
	~ClientSocket() {}

private:
	SOCKET _sockfd;
	int _lastpos ;	//��Ϣ������������β��λ��
	char _szMsgBuf[RECV_BUFF_SIZE * 10];	//�ڶ�������
};



class EasyTcpServer
{
private:
	SOCKET _sock;
	std::vector<ClientSocket*> _clients;  //������е�socket
	CELLTimestamp _tTime;
	int _recvCount;
public:
	EasyTcpServer() 
	{
		_sock = INVALID_SOCKET;
		_recvCount = 0;
	}
	virtual ~EasyTcpServer()
	{
		Close();
	}

	//��ʼ��socket
	void InitSocket()
	{
		//����window socket �Ļ���
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		//1������һ���׽���
		if (INVALID_SOCKET != _sock)
		{
			cout << "Socket = " << _sock << " �������Ѿ��ر�" << endl;
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			cout << "���󣬽���Socket = " << _sock << " ʧ��..." << endl;
		}
		else
		{
			cout << "����Socket = " << _sock << " �ɹ�..." << endl;
		}
	}
	
	//�󶨶˿�
	void BindSocket(const char* ip,unsigned short port)
	{
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		if (ip)	//ip��Ϊ��
		{
			_sin.sin_addr.S_un.S_addr = inet_addr(ip); //���� inet_addr("127.0.0.1"
		}
		else
		{
			_sin.sin_addr.S_un.S_addr = INADDR_ANY; //���� inet_addr("127.0.0.1")
		}
#else
		if (ip)
		{
			_sin.sin_addr.s_addr = inet_addr(ip);      //���� inet_addr("127.0.0.1")
		}
		else
		{
			_sin.sin_addr.s_addr = INADDR_ANY ;      //���� inet_addr("127.0.0.1")
		}		
#endif // _WIN32
		
		int ret =::bind(_sock, (sockaddr*)&_sin, sizeof(_sin));		//����std�е�bing���socket�е�bind��ͻ���������::���з���
		if (SOCKET_ERROR == ret)
		{
			cout << "Socket = " << _sock << " �󶨶˿�ʧ��" << endl;
		}
		else
		{
			cout << "Socket = " << _sock << " �󶨶˿ڳɹ�" << endl;
		}
	}
	//�����˿�
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			cout << "Socket = " << _sock << " �����˿�ʧ��" << endl;
		}
		else
		{
			cout << "Socket = " << _sock << " �����˿ڳɹ�" << endl;
		}
		return ret;
	}
	//���ܿͻ�������
	SOCKET Accept()
	{
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif // _WIN32

		if (INVALID_SOCKET == cSock)
		{
			cout << "Socket = " << cSock << "���󣬽��յ���Ч�ͻ���" << endl;
		}
		else
		{
			NewUserJoin userjoin;
			SendDataToAll(&userjoin);
			ClientSocket* newSock =new  ClientSocket(cSock);
			_clients.push_back(newSock);    //ֱ�ӽ������װ�Ŀͻ������Ӽ���
			cout<< "�µĿͻ��˼��� Socket = " << cSock <<endl;
		}
		return cSock;
	}

	//����������Ϣ
	bool OnRun()
	{
		if (isRun())
		{
			//�������׽��� BSD socket
			fd_set fdRead;
			fd_set fdWrite;
			fd_set fdExp;

			FD_ZERO(&fdRead);
			FD_ZERO(&fdWrite);
			FD_ZERO(&fdExp);

			FD_SET(_sock, &fdRead);
			FD_SET(_sock, &fdWrite);
			FD_SET(_sock, &fdExp);
			SOCKET maxSock = _sock;
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(_clients[n]->sockfd() , &fdRead);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}

			//nfds��һ������ֵ����ָfd_set������������������socket���ķ�Χ��������������
			//���������ļ����������ֵ+1����window�У���ֵ����д0   ע��  �����ֵ��һ������
			timeval t = { 1,0 };    //��ʱ
			int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);
			if (ret < 0)
			{
				cout << "select �������" << endl;
				Close();
				return false;
			}
			if (FD_ISSET(_sock, &fdRead))   //FD_ISSET:�ж�һ���ļ��������Ƿ���һ�������У�����ֵ:��1,����0
			{
				FD_CLR(_sock, &fdRead); //FD_CLR:��һ���ļ��������Ӽ������Ƴ�
				Accept();
				return true;	//�������꣬Ȼ���ٴ������� ���� ����Ϊ�˲���
			}

			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead))
				{
					int result = RecvData(_clients[n]);           //�������ܵ���Ϣ���׽��ֵ���Ϣ
					if (-1 == result)       //˵���г����˳���  ���Ǿ�Ӧ���ҵ���� Ȼ���Ƴ���
					{
						auto iter = _clients.begin() + n;
						if (iter != _clients.end())
						{
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
		}
		return true;
	}

	//�ж��Ƿ�������
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}
	//�Ƿ��ڹ�����
	//��������  ����ճ�� ��ְ�
	int RecvData(ClientSocket* pClient)            //ר�Ŵ������ܵ�����Ϣ
	{
		//������
		char szRecv[4096] = {};
		int len = (int)recv(pClient->sockfd(), szRecv, sizeof(DataHeader), 0);
		DataHeader* header = (DataHeader*)szRecv;
		if (len <= 0)
		{
			cout<<"Sokect = "<< pClient->sockfd()<< " �Ѿ��˳����������"<<endl;
			return -1;
		}

		//����ȡ�������ݿ�������Ϣ������
		::memcpy(pClient->msgBuf()+pClient->getlast(), szRecv, len);
		//����Ϣ������������β��λ�ú���
		pClient->setLast(pClient->getlast() + len);
		//ѭ���ж���Ϣ�������ĳ����Ƿ������Ϣ����
		while (pClient->getlast() >= sizeof(DataHeader))	//���ճ��
		{
			//��ʱ��Ϳ���֪����ǰ��Ϣ�ĳ���
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//�ж���Ϣ�������ĳ��ȴ�����Ϣ����
			if (pClient->getlast() >= header->dataLength)		//����ٰ�
			{
				//��Ϣ������ʣ��δ���������ݵĳ���
				int nSize = pClient->getlast() - header->dataLength;

				//����������Ϣ
				OnNetMsg(pClient->sockfd(), header);
				//����Ϣ������ʣ�������ǰ��
				::memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//��Ϣ������������β����ǰ�ƶ�
				pClient->setLast( nSize);
			}
			else
			{
				//ʣ����Ϣ����һ������Ϣ
				break;
			}
		}
		return 0;
	}

	//��Ӧ������Ϣ
	virtual void OnNetMsg(SOCKET cSock,DataHeader* header)
	{
		_recvCount++;
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			cout << "t1 =  "<<t1<<" ,socket = "<<cSock <<" ,recvCount = "<<_recvCount<< endl;
			_tTime.update();
			_recvCount = 0;
		}
		switch (header->cmd)
		{
			case CMD_LOGIN:
			{
				//ע�� ����ΪʲôҪ�� sizeof(DataHe ader)�ͼ�ȥsizeof(DataHeader)  ����Ϊ ǰ�� �����Ѿ�������һ��ͷ  ���� ����Ҫ����ַƫ��
				
				Login *login = (Login*)header;
				//cout<<"�յ��ͻ��� "<<cSock<<" �����CMD_LOGIN, ���ݳ���= "<< login->dataLength <<" ,userName = "<< login->userName <<" Password = "<< login->passWord<<endl;
				//�����û������Ƿ���ȷ�Ĺ���
				LoginResult ret;
				SendData(cSock, &ret);
				break;
			}
			case CMD_LOGOUT:
			{
				Logout *logout = (Logout*)header;
				//cout << "�յ��ͻ��� " << cSock << " �����CMD_LOGOUT, ���ݳ���= " << logout->dataLength << " ,userName = " << logout->username << endl;
				//�����û������Ƿ���ȷ�Ĺ���
				//LogoutResult ret;
				//SendData(cSock, &ret);
				break;
			}
			default:
			{
				cout << "�յ� "<< cSock << " δ�������Ϣ ���ݳ���: " << header->dataLength << endl;
				//DataHeader ret;
				//SendData(cSock,&ret);
				break;
			}
		}
	}

	//��ָ��������
	int SendData(SOCKET cSock,DataHeader* header)
	{
		if (isRun() && header)
		{
			send(cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	//Ⱥ��������
	void SendDataToAll( DataHeader* header)
	{
		//�µĿͻ��˻�û�м��뵽vector֮�󣬾���Ⱥ��������������Ա
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{
			SendData(_clients[n]->sockfd(), header);
		}
	}
	//�ر�socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
			for (int n = (int)_clients.size() - 1; n >= 0; n--)    //������������˳�����for����ǲ���õ�ִ�е�
			{
#ifdef _WIN32
				closesocket(_clients[n]->sockfd());      //�ر�
				delete _clients[n];	//����new��֮�� Ҫ�ǵùر� ��Ȼ�ᵼ���ڴ�й©
#else
				close(_clients[n].sockfd() );
				delete _clients[n];
#endif
			}

			//6���ر��׽���
#ifdef _WIN32
			WSACleanup();
			closesocket(_sock);
#else
			close(_sock);
#endif
			_clients.clear();
		}
	}



};




#endif // !_EasyTcpServer_hpp_
