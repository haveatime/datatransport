
#include <QDebug>
#include "dtlocalbusplugin.h"

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

QSharedPointer<QObject> DtLocalBusFactory::createOneDataTransportInstance( const QVariantHash &parameter )
{
	return QSharedPointer<QObject>(new DtLocalBusPlugin(parameter));
}

#if QT_VERSION<QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(staticdtlocalbusfactory, DtLocalBusFactory)
#endif

DtLocalBusPlugin::DtLocalBusPlugin( const QVariantHash &parameter )
:DataTransportInterface(parameter),groupmode(0)
{
	if (getDebug() > 0)
		qDebug() << "DtLocalBusPlugin::DtLocalBusPlugin parameter=" << parameter;

	groupport = parameter["groupport"].toInt();
	if( groupport == 0 )
	{
		qDebug() << "DtLocalBusPlugin::DtLocalBusPlugin error groupport is 0.";
		return;	
	}

	groupmode = parameter["groupmode"].toInt();
	if( groupmode < LOCAL_BUS_USE_LIMITED_BROADCAST || groupmode > LOCAL_BUS_USE_NETWROK_MULTICAST   )
	{
		qDebug() << "DtLocalBusPlugin::DtLocalBusPlugin error groupmode is wrong.";
		return;	
	}

	bool flag;

	if( groupmode == LOCAL_BUS_USE_LIMITED_BROADCAST )
	{
		groupaddress = QHostAddress::Broadcast;
#if QT_VERSION>=QT_VERSION_CHECK(5,0,0)
			flag = groupsocket.bind(QHostAddress::AnyIPv4, groupport, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
#else
			flag = groupsocket.bind(QHostAddress::Any, groupport, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
#endif
	}
	else
	{
		groupaddress = parameter["groupaddr"].toString();
		if (groupaddress.toString().isEmpty())
		{
			qDebug() << "DtLocalBusPlugin::DtLocalBusPlugin error groupaddress is empty.";
			return;	
		}

#if QT_VERSION>=QT_VERSION_CHECK(5,0,0)
			flag = groupsocket.bind(QHostAddress::AnyIPv4, groupport, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
#else
			flag = groupsocket.bind(QHostAddress::Any, groupport, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
#endif
		if( groupmode == LOCAL_BUS_USE_LOOPBACK_MULTICAST )
		{
#ifdef WIN32
			SOCKET s = groupsocket.socketDescriptor();
			struct ip_mreq  ipmr;
			ipmr.imr_multiaddr.s_addr = inet_addr(groupaddress.toString().toStdString().c_str());
			ipmr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);
			int ret = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&ipmr, sizeof(ipmr));
#else
			struct in_addr local_addr;
			local_addr.s_addr = htonl(INADDR_LOOPBACK);
			int setresult = setsockopt(groupsocket.socketDescriptor(),IPPROTO_IP,IP_MULTICAST_IF,(char*)&local_addr,sizeof(local_addr));

			QNetworkInterface loopbackinterface = getLoopBackInterface();
			bool valid = loopbackinterface.isValid();
			flag = groupsocket.joinMulticastGroup(groupaddress,loopbackinterface);
			/* 由于loopbackinterface的QNetworkAddressEntry中共有两个,
	   		如果第一个是IPv6类型的,那么在join中就会被toIPv4Address为0,
	   		在网络连接属性中不勾选IPv6也解决不了此问题 */
#endif
		}
		else if( groupmode == LOCAL_BUS_USE_NETWROK_MULTICAST )
		{
			flag = groupsocket.joinMulticastGroup(groupaddress);
			/*
	  		join时最好能指定具体的网络接口地址(即使是127网段的也行,见上INADDR_LOOPBACK),
	  		否则在使用AnyIPv4就得必须保证设置了多播路由.
	  		route add -net 224.0.0.0/4 gw `hostname`
			*/
		}
		else
		{
			qDebug() << "MsgBusIf::MsgBusIf error groupmode=" << groupmode;			
		}

		groupsocket.setSocketOption(QAbstractSocket::MulticastTtlOption, 0);/* 本地主机 */
		groupsocket.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);/* 接收本机  */
	}

	connect(&groupsocket, SIGNAL(readyRead()), this, SLOT(processDatagrams()));
	connect(&groupsocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(processErrors(QAbstractSocket::SocketError)));
}

DtLocalBusPlugin::~DtLocalBusPlugin()
{
	groupsocket.close();
}

void DtLocalBusPlugin::processErrors(QAbstractSocket::SocketError socketError)
{
	qDebug() << "DtLocalBusPlugin::processErrors=" << socketError;
}

void DtLocalBusPlugin::processDatagrams()
{
	QList<QByteArray> recvlist;

	while (groupsocket.hasPendingDatagrams())
	{
		QByteArray data;
		data.resize(groupsocket.pendingDatagramSize());
		groupsocket.readDatagram(data.data(), data.size());

		recvlist.append(data);
	}

	if( recvlist.size() > 0 )
		emit readData(recvlist);
}

void DtLocalBusPlugin::writeData(const QList<QByteArray> &data)
{
	int count = data.size();

	for( int index=0; index<count; index++ )
	{
		const QByteArray &frame = data.at(index);
		groupsocket.writeDatagram(frame.data(), frame.size(), groupaddress, groupport);
	}
}

QNetworkInterface DtLocalBusPlugin::getLoopBackInterface()
{
		QNetworkInterface netinterface;

		QList<QNetworkInterface> hostinterface = QNetworkInterface::allInterfaces();

		for (QList<QNetworkInterface>::iterator index = hostinterface.begin(); index != hostinterface.end(); index++)
		{
			if (index->flags() & QNetworkInterface::IsLoopBack)
			{
				netinterface = *index;

				QList<QNetworkAddressEntry> addressEntries = netinterface.addressEntries();
				for (QList<QNetworkAddressEntry>::iterator entry = addressEntries.begin();
					entry != addressEntries.end(); entry++)
				{
					QHostAddress ipentry = entry->ip();
					QAbstractSocket::NetworkLayerProtocol ipprotocol = ipentry.protocol();
					if( ipprotocol == QAbstractSocket::IPv4Protocol )
						qDebug() << "ipprotocol=" << ipprotocol <<"ipaddress=" << ipentry.toIPv4Address();
					else if( ipprotocol == QAbstractSocket::IPv6Protocol )
					{
						Q_IPV6ADDR ipv6addr = ipentry.toIPv6Address();
						qDebug() << "ipprotocol=" << ipprotocol <<"ipaddress=" << 
						ipv6addr[0]<<ipv6addr[1]<<ipv6addr[2]<<ipv6addr[3]<<ipv6addr[4]<<ipv6addr[5]<<ipv6addr[6]<<ipv6addr[7] <<
						ipv6addr[8]<<ipv6addr[9]<<ipv6addr[10]<<ipv6addr[11]<<ipv6addr[12]<<ipv6addr[13]<<ipv6addr[14]<<ipv6addr[15];
					}
					else
						qDebug() << "ipprotocol=unknown";
				}

				break;
			}
		}

		return netinterface;
}
