
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
 �鲥��ַ��Χ224.0.0.0-239.255.255.255
 ����224.0.0.0-224.0.0.255ΪԤ���鲥��ַ,����224.0.0.1��ָ��������,224.0.0.2��ָ����·����
 224.0.1.0-224.0.1.255Ϊ�����鲥��ַ,��������internet
 224.0.2.0-238.255.255.255Ϊ�û����õ��鲥��ַ,ȫ����Χ����Ч
 239.0.0.0-239.255.255.255Ϊ���ع����鲥��ַ,�����ض��ı��ط�Χ����Ч
********************************************************/

/********************************************************
 1 Linux3.9�汾���鲥socket��bindʱ�Ƽ�ʹ��SO_REUSEPORTѡ��,������Ϣ����strace -e bind �������̺�鿴.
 2 bind�˿�����ʱ,�����������̵��û�Ȩ�����.
 3 bindʱ����ʹ��QHostAddress::Any���ص�ַ(Qt5ʹ��QHostAddress::AnyIPv4),Ȼ��IP_MULTICAST_IF��IP_ADD_MEMBERSHIP��ʹ��htonl(INADDR_LOOPBACK).
 4 ��linux�Ϸ�����ͨ�㲥�����޹㲥,��bindʹ��QHostAddress::Anyʱ�����Լ��յ��㲥,��bindʹ��QHostAddress::Localhostʱ�Լ�ȴ�ղ���.��windows���Լ��������յ�.
 5 writeʱ����ʹ��QHostAddress::Broadcast����(����)�㲥��ַ,�������·��route add -host 255.255.255.255 dev loʹ��Ϣ���ڱ��ؽ���.
 6 MulticastTtlOption=0ʱʹ���鲥��Ϣֻ���ڱ�������.
 7 �����鲥·��route add -net 224.0.0.0/4 dev eth0
 8 netstat -gn��ʾ224.0.0.1ʱ˵���ýӿھ����鲥����.
 9 netstat -gn���Բ鿴��ǰ������鲥���ĸ������ӿ���,windows��ʹ������netsh interface ipv4 show joins
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
