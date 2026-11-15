"""make_license.py - VestaHome's support license (vlicense.h): the author's side.

  keygen <private.pem> <vlicense_key.inc>   a new key pair: the private key (never into a repo - VestaCore is public),
                                            the public one as C bytes for vlicense.cpp
  sign <private.pem> <VH-XXXXX-XXXXX> <VestaLicense.key>   the license file for one head unit's ID
  id <hex of the unit's HAL device id>      the ID VestaHome shows for those bytes (LicenseIdText)

The file: "VestaHome support license", "id=<ID>", "sig=<base64 of r||s>" - ECDSA P-256 over SHA-256 of
"VestaHome support 1|<ID>". Needs the "cryptography" package.
"""
import base64, hashlib, os, sys
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

ALPHABET = '0123456789ABCDEFGHJKMNPQRSTVWXYZ'   # (no I, L, O, U: read off a screen without mistakes)


def unit_id(hw):
    d = hashlib.sha256(hw).digest()
    v = int.from_bytes(d[:7], 'big') >> 6   # 50 bits: 10 characters
    s = ''.join(ALPHABET[(v >> (5 * (9 - i))) & 31] for i in range(10))
    return 'VH-%s-%s' % (s[:5], s[5:])


def message(uid):
    return ('VestaHome support 1|' + uid).encode('ascii')


def sign(priv_path, uid):
    key = serialization.load_pem_private_key(open(priv_path, 'rb').read(), password=None)
    r, s = decode_dss_signature(key.sign(message(uid), ec.ECDSA(hashes.SHA256())))
    sig = r.to_bytes(32, 'big') + s.to_bytes(32, 'big')
    return 'VestaHome support license\nid=%s\nsig=%s\n' % (uid, base64.b64encode(sig).decode('ascii'))


def keygen(priv_path, inc_path):
    if os.path.exists(priv_path):
        sys.exit('%s exists - not overwritten (the licenses made with it would stop working)' % priv_path)
    key = ec.generate_private_key(ec.SECP256R1())
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                            serialization.NoEncryption())
    os.makedirs(os.path.dirname(os.path.abspath(priv_path)), exist_ok=True)
    open(priv_path, 'wb').write(pem)
    write_inc(key.public_key(), inc_path)


def write_inc(pub, inc_path):
    q = pub.public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    rows = ['    ' + ', '.join('0x%02X' % b for b in q[i:i + 13]) + ',' for i in range(0, len(q), 13)]
    open(inc_path, 'w', newline='\n').write(
        '// vlicense_key.inc - the license key\'s public half (tools/make_license.py keygen): P-256, uncompressed\n'
        'static const unsigned char kLicensePub[65] = {\n' + '\n'.join(rows) + '\n};\n')


def main(a):
    if len(a) == 3 and a[0] == 'keygen':
        keygen(a[1], a[2])
    elif len(a) == 4 and a[0] == 'sign':
        open(a[3], 'w', newline='\n').write(sign(a[1], a[2]))
    elif len(a) == 2 and a[0] == 'id':
        print(unit_id(bytes.fromhex(a[1])))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
