
#include <QDebug>
#include "dtmulticastplugin.h"

QSharedPointer<QObject> DtMultiCastFactory::createOneDataTransportInstance( const QVariantHash &parameter )
{
	return QSharedPointer<QObject>(new DtMultiCastPlugin(parameter));
}

#if QT_VERSION<QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(staticdtmulticastfactory, DtMultiCastFactory)
#endif

DtMultiCastPlugin::DtMultiCastPlugin( const QVariantHash &parameter )
:DataTransportInterface(parameter)
{
	if (getDebug() > 0)
		qDebug() << "DtMultiCastPlugin::DtMultiCastPlugin parameter=" << parameter;

	mcaddress = parameter["mcaddress"].toString();
	mcport = parameter["mcport"].toInt();
	loopbackvalue = parameter["loopbackvalue"].toInt();

#if QT_VERSION>QT_VERSION_CHECK(5,0,0)
	if (!QHostAddress(mcaddress).isMulticast())
	{
		qDebug() << "DtMultiCastPlugin mcaddress=" << mcaddress << "isnt multicast!";
		return;
	}
#endif

/********************************************************
 组播地址范围224.0.0.0-239.255.255.255
 其中224.0.0.0-224.0.0.255为预留组播地址,例如224.0.0.1特指所有主机,224.0.0.2特指所有路由器
 224.0.1.0-224.0.1.255为公用组播地址,可以用于internet
 224.0.2.0-238.255.255.255为用户可用的组播地址,全网范围内有效
 239.0.0.0-239.255.255.255为本地管理组播地址,仅在特定的本地范围内有效
********************************************************/

/********************************************************
 1 Linux3.9版本后组播socket在bind时推荐使用SO_REUSEPORT选项,具体信息可以strace -e bind 启动进程后查看.
 2 bind端口重用时,还与启动进程的用户权限相关.
 3 bind时可以使用QHostAddress::Any本地地址(Qt5使用QHostAddress::AnyIPv4),然后IP_MULTICAST_IF和IP_ADD_MEMBERSHIP都使用htonl(INADDR_LOOPBACK).
 4 在linux上发送普通广播或受限广播,当bind使用QHostAddress::Any时可以自己收到广播,但bind使用QHostAddress::Localhost时自己却收不到.在windows上自己都可以收到.
 5 write时可以使用QHostAddress::Broadcast本地(受限)广播地址,可以添加路由route add -host 255.255.255.255 dev lo使信息仅在本地接收.
 6 MulticastTtlOption=0时使得组播消息只限于本地主机.
 7 增加组播路由route add -net 224.0.0.0/4 dev eth0
 8 netstat -gn显示224.0.0.1时说明该接口具有组播功能.
 9 netstat -gn可以查看当前加入的组播在哪个网卡接口上,windows下使用命令netsh interface ipv4 show joins
********************************************************/
#if QT_VERSION>QT_VERSION_CHECK(5,0,0)
	bool bindifok = mcsocket.bind(QHostAddress::AnyIPv4, mcport, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);
#else
	bool bindifok = mcsocket.bind(QHostAddress::Any, mcport, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
#endif
	if (!bindifok)
	{
		qDebug() << "mcaddress=" << mcaddress << "mcport=" << mcport << "bind error=" << mcsocket.errorString();
		return;
	}

	mcsocket.joinMulticastGroup(QHostAddress(mcaddress));
	mcsocket.setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
	mcsocket.setSocketOption(QAbstractSocket::MulticastLoopbackOption, loopbackvalue);

	connect(&mcsocket, SIGNAL(readyRead()), this, SLOT(processDatagrams()));
	connect(&mcsocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(processErrors(QAbstractSocket::SocketError)));
}

DtMultiCastPlugin::~DtMultiCastPlugin()
{
	mcsocket.close();
}

void DtMultiCastPlugin::processErrors(QAbstractSocket::SocketError socketError)
{
	qDebug() << "DtMultiCastPlugin::processErrors=" << socketError;
}

void DtMultiCastPlugin::processDatagrams()
{
	QList<QByteArray> recvlist;

	while (mcsocket.hasPendingDatagrams())
	{
		QByteArray data;
		data.resize(mcsocket.pendingDatagramSize());
		mcsocket.readDatagram(data.data(), data.size());

		recvlist.append(data);
	}

	if( recvlist.size() > 0 )
		emit readData(recvlist);
}

void DtMultiCastPlugin::writeData(const QList<QByteArray> &data)
{
	int count = data.size();

	for( int index=0; index<count; index++ )
	{
		const QByteArray &frame = data.at(index);
		mcsocket.writeDatagram(frame.data(), frame.size(), QHostAddress(mcaddress), mcport);
	}
}
