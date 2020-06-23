
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
    <һ> linx������������eth0��ip��ַ192.168.1.100,��udp����:
		����192.168.1.255:22222
		bind   0.0.0.0          �㲥�ɹ�,�Լ�Ҳ���յ�     ͨ��eth0����
		bind   127.0.0.1        ����ʧ��,errno=22(EINVAL)
		bind   192.168.1.100   �㲥�ɹ�,�Լ�ȴû���յ�   ͨ��eth0����
		����192.168.1.100:22222
		bind   0.0.0.0          ���ͳɹ�,�Լ�Ҳ���յ�      ͨ��lo����
		bind   127.0.0.1        ���ͳɹ�,�Լ�ȴû���յ�    ͨ��lo����
		bind   192.168.1.100   ���ͳɹ�,�Լ�Ҳ���յ�      ͨ��lo����
		����255.255.255.255:22222
		bind   0.0.0.0          �㲥�ɹ�,�Լ�Ҳ���յ�      ͨ��eth0����
		bind   127.0.0.1        ���ͳɹ�,�Լ�ȴû���յ�    ͨ��lo����
		bind   192.168.1.100   �㲥�ɹ�,�Լ�ȴû���յ�    ͨ��eth0����

    <��> windows������������ip��ַ192.168.1.101,��һ��udp����:
		����192.168.1.255:22222
		bind   0.0.0.0          �㲥�ɹ�,�Լ�Ҳ���յ�      ͨ��������������
		bind   127.0.0.1        ���ͳɹ�,�Լ�ȴû���յ�    ͨ��lo����
		bind   192.168.1.101    �㲥�ɹ�,�Լ�Ҳ���յ�      ͨ��������������
		����192.168.1.101:22222
		bind   0.0.0.0          ���ͳɹ�,�Լ�Ҳ���յ�      ͨ��lo����
		bind   127.0.0.1        ���ͳɹ�,�Լ�ȴû���յ�    ͨ��lo����
		bind   192.168.1.101    ���ͳɹ�,�Լ�Ҳ���յ�      ͨ��lo����
		����255.255.255.255:22222
		bind   0.0.0.0          ���ͳɹ�,�Լ�Ҳ���յ�      ͨ��������������
		bind   127.0.0.1        ���ͳɹ�,�Լ�Ҳ���յ�      ͨ��lo����
		bind   192.168.1.101    ���ͳɹ�,�Լ�Ҳ���յ�      ͨ��������������

 ��������:
 1 �������ݰ�ʱ,�Ǹ���Ŀ��ip���������ĸ�����,Ҳ������Ҫȥ����·�ɱ�,��bind�ĸ�ip����û�й�ϵ
 2 ֻ�д�lo�������͵����ݰ��ſ��Իش�
 3 udp����bind�˿�22222,Ȼ�������ݰ�������ip��ַ+�˿�22222,ʵ���Է�����
 4 0.0.0.0��������������,127.0.0.1������lo����,����ip��ַ����lo�������Ǹ����õ�����
 5 ���Ҫ���͹㲥���ݰ�,��Ҫsetsockopt SO_BROADCASTѡ��
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
