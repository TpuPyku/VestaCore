"""test_make_license.py - make_license.py: the unit's ID as typed, the key's path, the signed file.

    python VestaCore/tools/test_make_license.py
"""
import base64
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_license as ml  # noqa: E402

from cryptography.exceptions import InvalidSignature  # noqa: E402
from cryptography.hazmat.primitives import hashes, serialization  # noqa: E402
from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402
from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature  # noqa: E402


class ParseId(unittest.TestCase):
    def test_as_shown(self):
        self.assertEqual(ml.parse_id('VH-EQFK3-005Z8'), 'VH-EQFK3-005Z8')

    def test_typed_loosely(self):   # lower case, spaces, no prefix or dashes
        self.assertEqual(ml.parse_id('  vh-eqfk3-005z8 '), 'VH-EQFK3-005Z8')
        self.assertEqual(ml.parse_id('EQFK3005Z8'), 'VH-EQFK3-005Z8')
        self.assertEqual(ml.parse_id('eqfk3 005z8'), 'VH-EQFK3-005Z8')

    def test_look_alikes(self):   # the alphabet has no O, I, L, U: what was meant
        self.assertEqual(ml.parse_id('VH-EQFK3-OO5Z8'), 'VH-EQFK3-005Z8')
        self.assertEqual(ml.parse_id('VH-IL000-00000'), 'VH-11000-00000')

    def test_not_an_id(self):
        for bad in ('', 'VH-EQFK3-005Z', 'VH-EQFK3-005Z89', 'VH-EQFK3-005ZU', 'XX-EQFK3-005Z8', 'VH-EQFK3-005Z!'):
            self.assertIsNone(ml.parse_id(bad), bad)

    def test_unit_id_round_trip(self):
        uid = ml.unit_id(b'\x01\x02\x03')
        self.assertEqual(ml.parse_id(uid), uid)


class KeyPath(unittest.TestCase):
    def test_env_and_home_expanded(self):   # (PowerShell passes %USERPROFILE% as it is)
        os.environ['VL_TEST_DIR'] = r'C:\x'
        self.assertEqual(ml.key_path(r'%VL_TEST_DIR%\k.pem'), os.path.join(r'C:\x', 'k.pem'))
        self.assertEqual(ml.key_path('~/k.pem'), os.path.join(os.path.expanduser('~'), 'k.pem'))

    def test_default(self):
        self.assertEqual(ml.key_path(None),
                         os.path.join(os.path.expanduser('~'), '.vesta', 'vesta_license_private.pem'))


class Sign(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.priv = os.path.join(self.dir, 'p.pem')
        ml.keygen(self.priv, os.path.join(self.dir, 'k.inc'))
        pem = open(self.priv, 'rb').read()
        self.pub = serialization.load_pem_private_key(pem, password=None).public_key()

    def check(self, text, uid, level):
        lines = dict(l.split('=', 1) for l in text.splitlines()[1:])
        self.assertEqual(text.splitlines()[0], 'VestaHome support license')
        self.assertEqual(lines['id'], uid)
        self.assertEqual(lines.get('level', '1'), str(level))
        sig = base64.b64decode(lines['sig'])
        der = encode_dss_signature(int.from_bytes(sig[:32], 'big'), int.from_bytes(sig[32:], 'big'))
        self.pub.verify(der, ml.message(uid, level), ec.ECDSA(hashes.SHA256()))   # raises when wrong
        with self.assertRaises(InvalidSignature):
            self.pub.verify(der, ml.message(uid, 3 - level), ec.ECDSA(hashes.SHA256()))

    def test_subscriber(self):
        text = ml.sign(self.priv, 'VH-EQFK3-005Z8', 1)
        self.assertNotIn('level=', text)
        self.check(text, 'VH-EQFK3-005Z8', 1)

    def test_coauthor(self):
        text = ml.sign(self.priv, 'VH-EQFK3-005Z8', 2)
        self.assertIn('level=2\n', text)
        self.check(text, 'VH-EQFK3-005Z8', 2)

    def test_write_key(self):   # the GUI's step: the ID as typed, the file written
        out = os.path.join(self.dir, 'VestaLicense.key')
        uid = ml.write_key(self.priv, ' eqfk3-005z8 ', 2, out)
        self.assertEqual(uid, 'VH-EQFK3-005Z8')
        self.check(open(out, encoding='ascii').read(), uid, 2)

    def test_write_key_refuses(self):
        out = os.path.join(self.dir, 'none.key')
        with self.assertRaises(ValueError):
            ml.write_key(self.priv, 'VH-BAD', 1, out)
        with self.assertRaises(ValueError):
            ml.write_key(self.priv, 'VH-EQFK3-005Z8', 3, out)
        with self.assertRaises(FileNotFoundError):
            ml.write_key(os.path.join(self.dir, 'missing.pem'), 'VH-EQFK3-005Z8', 1, out)
        self.assertFalse(os.path.exists(out))


class SignInstaller(unittest.TestCase):   # the installer's signature (2026-11-25): VestaHome's "Обновить" checks it
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.priv = os.path.join(self.dir, 'p.pem')
        ml.keygen(self.priv, os.path.join(self.dir, 'k.inc'))
        pem = open(self.priv, 'rb').read()
        self.pub = serialization.load_pem_private_key(pem, password=None).public_key()
        self.exe = os.path.join(self.dir, 'VestaHomeInstall.exe')
        open(self.exe, 'wb').write(b'MZ...' + b'VHINSTVER:1.1.261130' + bytes(3) + b'tail')

    def test_signed(self):
        out = os.path.join(self.dir, 'VestaHomeInstall.sig')
        ver = ml.sign_installer(self.priv, self.exe, out)
        self.assertEqual(ver, '1.1.261130')
        lines = open(out, encoding='ascii').read().splitlines()
        self.assertEqual(lines[0], 'VestaHome installer signature')
        f = dict(l.split('=', 1) for l in lines[1:])
        import hashlib
        sha = hashlib.sha256(open(self.exe, 'rb').read()).hexdigest()
        self.assertEqual((f['version'], f['sha256']), ('1.1.261130', sha))
        sig = base64.b64decode(f['sig'])
        der = encode_dss_signature(int.from_bytes(sig[:32], 'big'), int.from_bytes(sig[32:], 'big'))
        self.pub.verify(der, ('VestaHome installer 1|1.1.261130|' + sha).encode(), ec.ECDSA(hashes.SHA256()))

    def test_no_mark(self):
        open(self.exe, 'wb').write(b'MZ no mark')
        with self.assertRaises(ValueError):
            ml.sign_installer(self.priv, self.exe, os.path.join(self.dir, 'x.sig'))
        self.assertFalse(os.path.exists(os.path.join(self.dir, 'x.sig')))

    def test_version_of(self):
        self.assertEqual(ml.installer_version(b'..VHINSTVER:1.2.3\x00..'), '1.2.3')
        self.assertIsNone(ml.installer_version(b'VHINSTVER:\x00'))
        self.assertIsNone(ml.installer_version(b'VHINSTVER:1.2'))   # (no end: not a mark)
        self.assertEqual(ml.installer_version(b'VHINSTVER:\x00 VestaHome inside VHINSTVER:2.0\x00'), '2.0')   # (VestaHome's empty mark inside the installer)


if __name__ == '__main__':
    unittest.main()
