"""make_radio_mirror.py: the paths as VestaHome's netcat.cpp makes them, the trimmed answers, the file's form."""
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_radio_mirror as m   # noqa: E402


class RadioMirror(unittest.TestCase):
    def test_paths_as_vestahome_asks(self):
        """netcat.cpp NcCatsPath / NcStationsPath, vjson.cpp UrlEncode (test_vjson: "jazz & blues/ä-_.~")."""
        self.assertEqual(m.cats_path('countries'), '/json/countries?order=stationcount&reverse=true&hidebroken=true')
        self.assertEqual(m.cats_path('tags'), '/json/tags?order=stationcount&reverse=true&hidebroken=true&limit=100')
        self.assertEqual(m.enc('jazz & blues/ä-_.~'), 'jazz%20%26%20blues%2F%C3%A4-_.~')
        self.assertEqual(m.stations_path('country', 'RU', 20),
                         '/json/stations/search?countrycode=RU&order=clickcount&reverse=true&hidebroken=true'
                         '&limit=20&offset=20')
        self.assertEqual(m.stations_path('tag', 'rock', 0, by_name=True),
                         '/json/stations/search?tag=rock&tagExact=true&order=name&reverse=false&hidebroken=true'
                         '&limit=20&offset=0')
        self.assertEqual(m.stations_path('lang', 'russian', 0),
                         '/json/stations/search?language=russian&languageExact=true&order=clickcount&reverse=true'
                         '&hidebroken=true&limit=20&offset=0')

    def test_plan(self):
        """The CIS's countries 5 pages in both orders, the others' first pages; the counts capped."""
        paths = m.plan(['US', 'RU', 'DE'], ['pop', 'rock'], ['russian'])
        self.assertEqual(len(paths), 5 * 2 + 2 + 2 * 2 + 1)
        self.assertIn(m.stations_path('country', 'RU', 80, by_name=True), paths)
        self.assertNotIn(m.stations_path('country', 'US', 20), paths)
        self.assertEqual(len(m.plan(['X%d' % i for i in range(100)], [], [])), m.COUNTRIES)

    def test_build_and_pack(self):
        """The lists and the pages trimmed to what VestaHome reads; a page no server gave - left out; the file:
        magic, then path, length, JSON."""
        st = [{'name': 'Радио', 'url': 'http://a', 'url_resolved': 'http://a/s', 'codec': 'MP3', 'bitrate': 128,
               'country': 'Russia', 'countrycode': 'RU', 'hls': 0, 'stationuuid': 'u1', 'favicon': 'x', 'votes': 9}]
        answers = {m.cats_path('countries'): [{'name': 'Russia', 'iso_3166_1': 'RU', 'stationcount': 9, 'x': 1}],
                   m.cats_path('languages'): [], m.cats_path('tags'): [{'name': 'pop', 'stationcount': 3}]}
        for p in range(m.CIS_PAGES):
            answers[m.stations_path('country', 'RU', p * m.PAGE)] = st
        tried = []

        def get(url):
            tried.append(url)
            path = url[url.index('/json'):]
            if path not in answers:
                raise OSError('none')
            return json.dumps(answers[path])
        got = m.build(get, sleep=lambda s: None)
        paths = [p for p, _ in got]
        self.assertEqual(paths[:3], [m.cats_path('countries'), m.cats_path('languages'), m.cats_path('tags')])
        self.assertEqual(len(got), 3 + m.CIS_PAGES)   # (by name, the tag's pages: none answered)
        self.assertEqual(json.loads(got[0][1]), [{'name': 'Russia', 'iso_3166_1': 'RU', 'stationcount': 9}])
        self.assertEqual(sorted(json.loads(got[3][1])[0]), sorted(m.STATION_KEYS))
        self.assertTrue(all(u.startswith('https://') for u in tried))
        self.assertEqual(len([u for u in tried if m.stations_path('tag', 'pop', 0) in u]), len(m.HOSTS))   # each server
        data = m.pack([('/a', b'[1]'), ('/b', '["я"]'.encode('utf-8'))])
        self.assertEqual(data, b'VHRM1\n/a\n3\n[1]/b\n6\n["\xd1\x8f"]')


if __name__ == '__main__':
    unittest.main()
