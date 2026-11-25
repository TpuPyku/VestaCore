r"""make_license.py - VestaHome's support license (vlicense.h): the author's side.

  keygen <private.pem> <vlicense_key.inc>   a new key pair: the private key (never into a repo - VestaCore is public),
                                            the public one as C bytes for vlicense.cpp
  sign <private.pem> <VH-XXXXX-XXXXX> <VestaLicense.key> [level]   the license file for one head unit's ID
                                            (the key's path may have %USERPROFILE% or ~; "-" - the default
                                            %USERPROFILE%\.vesta\vesta_license_private.pem);
                                            level 1 (default) - a subscriber: Android Auto's splash gone;
                                            2 - a co-author (Boosty «Соавторы»): also the smart home add-on
  id <hex of the unit's HAL device id>      the ID VestaHome shows for those bytes (LicenseIdText)
  sign-installer <private.pem> <VestaHomeInstall.exe> <VestaHomeInstall.sig>   the installer's signature (its
                                            version mark and SHA-256): VestaHome's "Обновить" runs only a signed one

The file: "VestaHome support license", "id=<ID>", ["level=<n>" when above 1,] "sig=<base64 of r||s>" - ECDSA P-256
over SHA-256 of "VestaHome support <level>|<ID>" (the keys made before levels, 2026-11-18, are level 1). Needs the
"cryptography" package.
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


def parse_id(text):
    """The unit's ID as typed (case, spaces, the "VH-" and the dashes free; O for 0, I and L for 1 - the alphabet
    has none of them): "VH-XXXXX-XXXXX", or None when it is not one."""
    s = ''.join(text.split()).upper().replace('-', '')
    if len(s) == 12 and s.startswith('VH'):
        s = s[2:]
    s = s.replace('O', '0').replace('I', '1').replace('L', '1')
    if len(s) != 10 or any(c not in ALPHABET for c in s):
        return None
    return 'VH-%s-%s' % (s[:5], s[5:])


def key_path(path):
    """The private key's path: %VAR% and ~ opened (PowerShell passes %USERPROFILE% as it is); None - the default."""
    if not path or path == '-':
        return os.path.join(os.path.expanduser('~'), '.vesta', 'vesta_license_private.pem')
    return os.path.normpath(os.path.expanduser(os.path.expandvars(path)))


def message(uid, level=1):
    return ('VestaHome support %d|%s' % (level, uid)).encode('ascii')


def sign(priv_path, uid, level=1):
    if level not in (1, 2):
        raise ValueError('level: 1 (subscriber) or 2 (co-author)')
    with open(priv_path, 'rb') as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    r, s = decode_dss_signature(key.sign(message(uid, level), ec.ECDSA(hashes.SHA256())))
    sig = r.to_bytes(32, 'big') + s.to_bytes(32, 'big')
    lvl = 'level=%d\n' % level if level > 1 else ''
    return 'VestaHome support license\nid=%s\n%ssig=%s\n' % (uid, lvl, base64.b64encode(sig).decode('ascii'))


def write_key(priv_path, typed_id, level, out_path):
    """The license for the ID as typed written to out_path; its ID back. ValueError: not an ID or not a level;
    the key not there - FileNotFoundError (nothing written then)."""
    uid = parse_id(typed_id)
    if not uid:
        raise ValueError('not a unit ID (VH-XXXXX-XXXXX): %r' % typed_id)
    text = sign(key_path(priv_path), uid, level)
    with open(out_path, 'w', newline='\n') as f:
        f.write(text)
    return uid


MARK = b'VHINSTVER:'   # the installer's version mark (VestaHomeInstall: "VHINSTVER:<version>" and a 0 byte)


def installer_version(data):
    """The version the installer's bytes carry; None - no mark (or an empty or unended one)."""
    at = data.find(MARK)
    while at >= 0:   # (the first one with a version: VestaHome inside the installer has the mark empty)
        end = data.find(bytes(1), at + len(MARK))
        ver = data[at + len(MARK):end] if end > 0 else b''
        if ver and len(ver) < 32:
            return ver.decode('ascii')
        at = data.find(MARK, at + 1)
    return None


def sign_installer(priv_path, exe_path, out_path):
    """VestaHomeInstall.sig for the installer (2026-11-25: VestaHome's "Обновить" runs it only with this): ECDSA P-256
    over SHA-256 of "VestaHome installer 1|<version>|<sha256 hex of the exe>". Its version back; ValueError - no
    version mark in the exe (nothing written)."""
    data = open(exe_path, 'rb').read()
    ver = installer_version(data)
    if not ver:
        raise ValueError('no version mark (VHINSTVER:) in %s' % exe_path)
    sha = hashlib.sha256(data).hexdigest()
    with open(key_path(priv_path), 'rb') as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    r, s = decode_dss_signature(key.sign(('VestaHome installer 1|%s|%s' % (ver, sha)).encode('ascii'),
                                         ec.ECDSA(hashes.SHA256())))
    sig = base64.b64encode(r.to_bytes(32, 'big') + s.to_bytes(32, 'big')).decode('ascii')
    with open(out_path, 'w', newline='\n') as f:
        f.write('VestaHome installer signature\nversion=%s\nsha256=%s\nsig=%s\n' % (ver, sha, sig))
    return ver


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
    elif len(a) in (4, 5) and a[0] == 'sign':
        try:
            print(write_key(a[1], a[2], int(a[4]) if len(a) == 5 else 1, a[3]))
        except (ValueError, OSError) as e:
            sys.exit(str(e))
    elif len(a) == 4 and a[0] == 'sign-installer':
        try:
            print(sign_installer(a[1], a[2], a[3]))
        except (ValueError, OSError) as e:
            sys.exit(str(e))
    elif len(a) == 2 and a[0] == 'id':
        print(unit_id(bytes.fromhex(a[1])))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
