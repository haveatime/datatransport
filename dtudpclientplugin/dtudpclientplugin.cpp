
#include <QDebug>
#include "dtudpclientplugin.h"

QSharedPointer<QObject> DtUdpClientFactory::createOneDataTransportInstance( const QVariantHash &parameter )
{
	return QSharedPointer<QObject>(new DtUdpClientPlugin(parameter));
}

#if QT_VERSION<QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(staticdtudpclientfactory, DtUdpClientFactory)
#endif

/***************************************************************************************************
    <一> linx机器配置网卡eth0的ip地址192.168.1.100,做udp测试:
		发给192.168.1.255:22222
		bind   0.0.0.0          广播成功,自己也能收到     通过eth0网卡
		bind   127.0.0.1        发送失败,errno=22(EINVAL)
		bind   192.168.1.100   广播成功,自己却没有收到   通过eth0网卡
		发给192.168.1.100:22222
		bind   0.0.0.0          发送成功,自己也能收到      通过lo网卡
		bind   127.0.0.1        发送成功,自己却没有收到    通过lo网卡
		bind   192.168.1.100   发送成功,自己也能收到      通过lo网卡
		发给255.255.255.255:22222
		bind   0.0.0.0          广播成功,自己也能收到      通过eth0网卡
		bind   127.0.0.1        发送成功,自己却没有收到    通过lo网卡
		bind   192.168.1.100   广播成功,自己却没有收到    通过eth0网卡

    <二> windows机器配置网卡ip地址192.168.1.101,做一下udp测试:
		发给192.168.1.255:22222
		bind   0.0.0.0          广播成功,自己也能收到      通过本地连接网卡
		bind   127.0.0.1        发送成功,自己却没有收到    通过lo网卡
		bind   192.168.1.101    广播成功,自己也能收到      通过本地连接网卡
		发给192.168.1.101:22222
		bind   0.0.0.0          发送成功,自己也能收到      通过lo网卡
		bind   127.0.0.1        发送成功,自己却没有收到    通过lo网卡
		bind   192.168.1.101    发送成功,自己也能收到      通过lo网卡
		发给255.255.255.255:22222
		bind   0.0.0.0          发送成功,自己也能收到      通过本地连接网卡
		bind   127.0.0.1        发送成功,自己也能收到      通过lo网卡
		bind   192.168.1.101    发送成功,自己也能收到      通过本地连接网卡

 初步结论:
 1 发送数据包时,是根据目的ip来决定走哪个网卡,也就是需要去查找路由表,与bind哪个ip本身没有关系
 2 只有从lo网卡发送的数据包才可以回传
 3 udp可以bind端口22222,然后发送数据包到本地ip地址+端口22222,实现自发自收
 4 0.0.0.0代表本地所有网卡,127.0.0.1代表本地lo网卡,主机ip地址代表lo网卡和那个配置的网卡
 5 如果要发送广播数据包,需要setsockopt SO_BROADCAST选项
***************************************************************************************************/

DtUdpClientPlugin::DtUdpClientPlugin( const QVariantHash &parameter )
:DataTransportInterface(parameter)
{
	if (getDebug() > 0)
		qDebug() << "DtUdpClientPlugin::DtUdpClientPlugin parameter=" << parameter;

	if (!parameter.contains("selfaddress"))
		selfaddress = "0.0.0.0";
	else
		selfaddress = parameter["selfaddress"].toString();

	selfport = parameter["selfport"].toInt();
	peeraddress = parameter["peeraddress"].toString();
	peerport = parameter["peerport"].toInt();

#if QT_VERSION>QT_VERSION_CHECK(5,0,0)
	bool bindifok = udpsocket.bind(QHostAddress(selfaddress), selfport, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);
#else
	bool bindifok = udpsocket.bind(QHostAddress(selfaddress), selfport, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
#endif
	if (!bindifok)
	{
		qDebug() << "selfaddress=" << selfaddress << "selfport=" << selfport << "bind error=" << udpsocket.errorString();
		return;
	}

	connect(&udpsocket, SIGNAL(readyRead()), this, SLOT(processDatagrams()));
	connect(&udpsocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(processErrors(QAbstractSocket::SocketError)));
}

DtUdpClientPlugin::~DtUdpClientPlugin()
{
	udpsocket.close();
}

void DtUdpClientPlugin::processErrors(QAbstractSocket::SocketError socketError)
{
	qDebug() << "DtUdpClientPlugin::processErrors=" << socketError;
}

void DtUdpClientPlugin::processDatagrams()
{
	QList<QByteArray> recvlist;

	while (udpsocket.hasPendingDatagrams())
	{
		QByteArray data;
		data.resize(udpsocket.pendingDatagramSize());
		udpsocket.readDatagram(data.data(), data.size());

		recvlist.append(data);
	}

	if( recvlist.size() > 0 )
		emit readData(recvlist);
}

void DtUdpClientPlugin::writeData(const QList<QByteArray> &data)
{
	int count = data.size();

	for( int index=0; index<count; index++ )
	{
		const QByteArray &frame = data.at(index);
		udpsocket.writeDatagram(frame.data(), frame.size(), QHostAddress(peeraddress), peerport);
	}
}
