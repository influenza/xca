/* vi: set sw=4 ts=4:
 *
 * Copyright (C) 2001 - 2011 Christian Hohnstaedt.
 *
 * All rights reserved.
 */



#include "pki_x509.h"
#include "pki_evp.h"
#include "func.h"
#include "x509name.h"
#include "exception.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <QDir>

#include "openssl_compat.h"

QPixmap *pki_x509req::icon[3] = { NULL, NULL, NULL };

pki_x509req::pki_x509req(const QString name)
	: pki_x509super(name)
{
	privkey = NULL;
	request = X509_REQ_new();
	pki_openssl_error();
	pkiType=x509_req;
	done = false;
}

const char *pki_x509req::getClassName() const
{
	return "pki_x509req";
}

pki_x509req::~pki_x509req()
{
	if (request)
		X509_REQ_free(request);
}

QSqlError pki_x509req::insertSqlData()
{
	XSqlQuery q;
	QSqlError e = pki_x509super::insertSqlData();
	if (e.isValid())
		return e;
	SQL_PREPARE(q, "INSERT INTO requests (item, hash, signed, request) "
		  "VALUES (?, ?, 0, ?)");
	q.bindValue(0, sqlItemId);
	q.bindValue(1, hash());
	q.bindValue(2, i2d_b64());
	q.exec();
	return q.lastError();
}

void pki_x509req::restoreSql(QSqlRecord &rec)
{
	pki_x509super::restoreSql(rec);
	QByteArray ba = QByteArray::fromBase64(
				rec.value(VIEW_x509req_request).toByteArray());
	d2i(ba);
	done = rec.value(VIEW_x509req_signed).toBool();
}

QSqlError pki_x509req::deleteSqlData()
{
	XSqlQuery q;
	QSqlError e = pki_x509super::deleteSqlData();
	if (e.isValid())
		return e;
	SQL_PREPARE(q, "DELETE FROM requests WHERE item=?");
	q.bindValue(0, sqlItemId);
	q.exec();
	return q.lastError();
}

void pki_x509req::createReq(pki_key *key, const x509name &dn, const EVP_MD *md, extList el)
{
	int bad_nids[] = { NID_subject_key_identifier, NID_authority_key_identifier,
		NID_issuer_alt_name, NID_undef };

	EVP_PKEY *privkey = NULL;

	if (key->isPubKey()) {
		my_error(tr("Signing key not valid (public key)"));
		return;
	}

	X509_REQ_set_version(request, 0L);
	X509_REQ_set_pubkey(request, key->getPubKey());
	setSubject(dn);
	pki_openssl_error();

	for(int i=0; bad_nids[i] != NID_undef; i++)
		el.delByNid(bad_nids[i]);

	el.delInvalid();

	if (el.count() > 0) {
		STACK_OF(X509_EXTENSION) *sk;
		sk = el.getStack();
		X509_REQ_add_extensions(request, sk);
		sk_X509_EXTENSION_pop_free(sk, X509_EXTENSION_free);
	}
	pki_openssl_error();

	privkey = key->decryptKey();
	X509_REQ_sign(request, privkey, md);
	pki_openssl_error();
	EVP_PKEY_free(privkey);
}

QString pki_x509req::getMsg(msg_type msg)
{
	/*
	 * We do not construct english sentences from fragments
	 * to allow proper translations.
	 * The drawback are all the slightly different duplicated messages
	 *
	 * %1 will be replaced by either "SPKAC" or "PKCS#10"
	 * %2 will be replaced by the internal name of the request
	 */

	QString type = "PKCS#10";

	switch (msg) {
	case msg_import: return tr("Successfully imported the %1 certificate request '%2'").arg(type);
	case msg_delete: return tr("Delete the %1 certificate request '%2'?").arg(type);
	case msg_create: return tr("Successfully created the %1 certificate request '%2'").arg(type);
	/* %1: Number of requests; %2: list of request names */
	case msg_delete_multi: return tr("Delete the %1 certificate requests: %2?");
	}
	return pki_base::getMsg(msg);
}

void pki_x509req::fromPEM_BIO(BIO *bio, QString name)
{
	X509_REQ *req;
	req = PEM_read_bio_X509_REQ(bio, NULL, NULL, NULL);
	openssl_error(name);
	X509_REQ_free(request);
	request = req;
	autoIntName();
	if (getIntName().isEmpty())
		setIntName(rmslashdot(name));
}

void pki_x509req::fload(const QString fname)
{
	FILE *fp = fopen_read(fname);
	X509_REQ *_req;
	int ret = 0;

	if (fp != NULL) {
		_req = PEM_read_X509_REQ(fp, NULL, NULL, NULL);
		if (!_req) {
			pki_ign_openssl_error();
			rewind(fp);
			_req = d2i_X509_REQ_fp(fp, NULL);
		}
		fclose(fp);
		if (ret || pki_ign_openssl_error()) {
			if (_req)
				X509_REQ_free(_req);
			throw errorEx(tr("Unable to load the certificate request in file %1. Tried PEM, DER and SPKAC format.").arg(fname));
		}
	} else {
		fopen_error(fname);
		return;
	}

	if (_req) {
		X509_REQ_free(request);
		request = _req;
	}
	autoIntName();
	if (getIntName().isEmpty())
		setIntName(rmslashdot(fname));
	openssl_error(fname);
}

void pki_x509req::d2i(QByteArray &ba)
{
	X509_REQ *r= (X509_REQ*)d2i_bytearray(D2I_VOID(d2i_X509_REQ), ba);
	if (r) {
		X509_REQ_free(request);
		request = r;
	}
}

QByteArray pki_x509req::i2d()
{
	return i2d_bytearray(I2D_VOID(i2d_X509_REQ), request);
}

void pki_x509req::fromData(const unsigned char *p, db_header_t *head )
{
	int size;

	size = head->len - sizeof(db_header_t);

	QByteArray ba((const char *)p, size);
	privkey = NULL;

	d2i(ba);
	pki_openssl_error();

	if (ba.count() > 0) {
		my_error(tr("Wrong Size %1").arg(ba.count()));
	}
}

void pki_x509req::addAttribute(int nid, QString content)
{
	if (content.isEmpty())
		return;

	ASN1_STRING *a = QStringToAsn1(content, nid);
	X509_REQ_add1_attr_by_NID(request, nid, a->type, a->data, a->length);
	ASN1_STRING_free(a);
	openssl_error(QString("'%1' (%2)").arg(content).arg(OBJ_nid2ln(nid)));
}

x509name pki_x509req::getSubject() const
{
	x509name x(X509_REQ_get_subject_name(request));
	pki_openssl_error();
	return x;
}

int pki_x509req::sigAlg()
{
	return X509_REQ_get_signature_nid(request);
}

void pki_x509req::setSubject(const x509name &n)
{
	X509_REQ_set_subject_name(request, n.get());
}

void pki_x509req::writeDefault(const QString fname)
{
	writeReq(get_dump_filename(fname, ".csr"), true);
}

void pki_x509req::writeReq(const QString fname, bool pem)
{
	FILE *fp = fopen_write(fname);
	if (fp) {
		if (request){
			if (pem)
				PEM_write_X509_REQ(fp, request);
			else
				i2d_X509_REQ_fp(fp, request);
		}
		fclose(fp);
		pki_openssl_error();
	} else
		fopen_error(fname);
}

BIO *pki_x509req::pem(BIO *b, int format)
{
	(void)format;
	if (!b)
		b = BIO_new(BIO_s_mem());
	PEM_write_bio_X509_REQ(b, request);
	return b;
}

int pki_x509req::verify()
{
	EVP_PKEY *pkey = X509_REQ_get_pubkey(request);
	bool x = X509_REQ_verify(request,pkey) > 0;
	pki_ign_openssl_error();
	EVP_PKEY_free(pkey);
	return x;
}

pki_key *pki_x509req::getPubKey() const
{
	 EVP_PKEY *pkey = X509_REQ_get_pubkey(request);
	 pki_ign_openssl_error();
	 if (pkey == NULL)
		 return NULL;
	 pki_evp *key = new pki_evp(pkey);
	 pki_openssl_error();
	 return key;
}

extList pki_x509req::getV3ext()
{
	extList el;
	STACK_OF(X509_EXTENSION) *sk;
	sk = X509_REQ_get_extensions(request);
	el.setStack(sk);
	sk_X509_EXTENSION_pop_free(sk, X509_EXTENSION_free);
	return el;
}

QString pki_x509req::getAttribute(int nid)
{
	int n;
	int count;
	QStringList ret;

	n = X509_REQ_get_attr_by_NID(request, nid, -1);
	if (n == -1)
		return QString("");
	X509_ATTRIBUTE *att = X509_REQ_get_attr(request, n);
	if (!att)
		return QString("");
	count = X509_ATTRIBUTE_count(att);
	for (int j = 0; j < count; j++)
		ret << asn1ToQString(X509_ATTRIBUTE_get0_type(att, j)->
				             value.asn1_string);
	return ret.join(", ");
}

QVariant pki_x509req::column_data(dbheader *hd)
{
	switch (hd->id) {
	case HD_req_signed:
		return QVariant(done ? tr("Signed") : tr("Unhandled"));
	case HD_req_unstr_name:
		return getAttribute(NID_pkcs9_unstructuredName);
	case HD_req_chall_pass:
		return getAttribute(NID_pkcs9_challengePassword);
	}
	return pki_x509super::column_data(hd);
}

QVariant pki_x509req::getIcon(dbheader *hd)
{
	int pixnum = -1;
	pki_key *k;

	switch (hd->id) {
	case HD_internal_name:
		pixnum = 0;
		k = getRefKey();
		if (k && k->isPrivKey())
			pixnum = 1;
		break;
	case HD_req_signed:
		if (done)
			pixnum = 2;
		break;
	}
	if (pixnum == -1)
		return QVariant();
	return QVariant(*icon[pixnum]);
}

bool pki_x509req::visible()
{
	if (pki_x509super::visible())
		return true;
	if (getAttribute(NID_pkcs9_unstructuredName).contains(limitPattern))
		return true;
	if (getAttribute(NID_pkcs9_challengePassword).contains(limitPattern))
		return true;
	return false;
}
