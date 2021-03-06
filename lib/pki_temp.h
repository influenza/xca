/* vi: set sw=4 ts=4:
 *
 * Copyright (C) 2001 - 2011 Christian Hohnstaedt.
 *
 * All rights reserved.
 */

#ifndef __PKI_TEMP_H
#define __PKI_TEMP_H

#include "pki_base.h"
#include "x509name.h"
#include "asn1time.h"
#include "pki_x509super.h"

#define D5 "-----"
#define PEM_STRING_XCA_TEMPLATE "XCA TEMPLATE"
#define TMPL_VERSION 10

#define CHECK_TMPL_KEY if (!tmpl_keys.contains(key)) { qDebug("Unknown template key: %s(%s)",  __func__, CCHAR(key)); }

#define VIEW_temp_version 6
#define VIEW_temp_template 7

class pki_temp: public pki_x509name
{
		Q_OBJECT
	protected:
		static QList<QString> tmpl_keys;
		int dataSize();
		void try_fload(QString fname, const char *mode);
		bool pre_defined;
		x509name xname;
		QMap<QString, QString> settings;
		QString adv_ext;
		void fromExtList(extList *el, int nid, const char *item);

	public:
		static QPixmap *icon;

		// methods
		const char *getClassName() const;
		QString getSetting(QString key)
		{
			CHECK_TMPL_KEY
			return settings[key];
		}
		int getSettingInt(QString key)
		{
			CHECK_TMPL_KEY
			return settings[key].toInt();
		}
		void setSetting(QString key, QString value)
		{
			CHECK_TMPL_KEY
			settings[key] = value;
		}
		void setSetting(QString key, int value)
		{
			CHECK_TMPL_KEY
			settings[key] = QString::number(value);
		}
		pki_temp(const pki_temp *pk);
		pki_temp(const QString d = QString());
		void fload(const QString fname);
		void writeDefault(const QString fname);
		~pki_temp();
		void fromData(const unsigned char *p, int size, int version);
		void old_fromData(const unsigned char *p, int size, int version);
		void fromData(const unsigned char *p, db_header_t *head );
		void fromData(QByteArray &ba, int version);
		void setAsPreDefined()
		{
			pre_defined = true;
		}
		QString comboText() const;
		QByteArray toData();
		QString toB64Data()
		{
			return QString::fromLatin1(toData().toBase64());
		}
		bool compare(pki_base *ref);
		void writeTemp(QString fname);
		QVariant getIcon(dbheader *hd);
		QString getMsg(msg_type msg);
		x509name getSubject() const;
		void setSubject(x509name n)
		{
			xname = n;
		}
		BIO *pem(BIO *b, int format);
		QByteArray toExportData();
		void fromPEM_BIO(BIO *, QString);
		void fromExportData(QByteArray data);
		extList fromCert(pki_x509super *cert_or_req);
		QSqlError insertSqlData();
		QSqlError deleteSqlData();
		void restoreSql(QSqlRecord &rec);
};

Q_DECLARE_METATYPE(pki_temp *);
#endif
