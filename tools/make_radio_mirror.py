"""The internet radio's catalogue mirrored (the user 2026-11-12: radio-browser.info is blocked for many - the stations
themselves play from their own addresses): radio-browser's answers to the very paths VestaHome asks (its netcat.cpp
NcCatsPath / NcStationsPath), trimmed to the fields it reads, in one file for a public release (VestaCore's "radio",
a weekly workflow). VestaHome downloads it weekly and answers from it when radio-browser does not.

    python make_radio_mirror.py out/radio-mirror.bin

The file: b"VHRM1\\n", then for each answer its path, "\\n", its length in bytes (decimal), "\\n", the JSON.
"""
import json
import sys
import time
import urllib.parse
import urllib.request

HOSTS = ('de1.api.radio-browser.info', 'de2.api.radio-browser.info', 'all.api.radio-browser.info')
UA = {'User-Agent': 'VestaHome-radio-mirror/1.0 (+https://github.com/TpuPyku/VestaCore)'}
PAGE = 20                       # netcat.h kNcPage
CIS = ('RU', 'BY', 'KZ', 'UZ', 'KG', 'TJ', 'TM', 'AM', 'AZ', 'MD')   # netcat.cpp InCis
CIS_PAGES, COUNTRIES, TAGS, TAG_PAGES, LANGS = 5, 40, 100, 2, 50
STATION_KEYS = ('name', 'url', 'url_resolved', 'codec', 'bitrate', 'country', 'countrycode', 'hls', 'stationuuid')
CAT_KEYS = ('name', 'iso_3166_1', 'stationcount')
MAGIC = b'VHRM1\n'


def enc(s):
    """vjson.cpp UrlEncode: all but letters, digits, -_.~ as %XX (UTF-8 bytes, upper-case hex)."""
    return urllib.parse.quote(s, safe='-_.~')


def cats_path(what):
    """netcat.cpp NcCatsPath: countries (also the CIS's list), languages, tags."""
    return '/json/%s?order=stationcount&reverse=true&hidebroken=true%s' % (what, '' if what == 'countries' else '&limit=100')


def stations_path(kind, arg, offset, by_name=False):
    """netcat.cpp NcStationsPath: kind 'tag', 'country' or 'lang'."""
    what = {'tag': 'tag=%s&tagExact=true&', 'country': 'countrycode=%s&',
            'lang': 'language=%s&languageExact=true&'}[kind] % enc(arg)
    return '/json/stations/search?%sorder=%s&reverse=%s&hidebroken=true&limit=%d&offset=%d' % (
        what, 'name' if by_name else 'clickcount', 'false' if by_name else 'true', PAGE, offset)


def trimmed(items, keys):
    return [{k: it[k] for k in keys if k in it} for it in items]


def get_json(path, get):
    """The answer from the first server that gives one; None: none did."""
    for host in HOSTS:
        try:
            return json.loads(get('https://' + host + path))
        except (OSError, ValueError):
            continue
    return None


def urllib_get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        return r.read().decode('utf-8')


def plan(countries, tags, langs):
    """The paths to mirror: the CIS's countries a few pages, both orders; the biggest other countries, tags and
    languages their first pages (by the clicks: the lists' order VestaHome opens with)."""
    paths = []
    cis = [c for c in CIS if c in countries]
    for c in cis:
        for p in range(CIS_PAGES):
            paths.append(stations_path('country', c, p * PAGE))
            paths.append(stations_path('country', c, p * PAGE, by_name=True))
    for c in [c for c in countries if c not in CIS][:COUNTRIES]:
        paths.append(stations_path('country', c, 0))
    for t in tags[:TAGS]:
        for p in range(TAG_PAGES):
            paths.append(stations_path('tag', t, p * PAGE))
    for l in langs[:LANGS]:
        paths.append(stations_path('lang', l, 0))
    return paths


def build(get=urllib_get, sleep=time.sleep):
    """[(path, JSON bytes)] - the three lists first, then the stations' pages; a page none answered is left out."""
    out = []
    lists = {}
    for what in ('countries', 'languages', 'tags'):
        items = get_json(cats_path(what), get) or []
        lists[what] = items
        out.append((cats_path(what), json.dumps(trimmed(items, CAT_KEYS), ensure_ascii=False).encode('utf-8')))
    countries = [c.get('iso_3166_1', '') for c in lists['countries'] if c.get('iso_3166_1')]
    names = lambda what: [c.get('name', '') for c in lists[what] if c.get('name')]   # noqa: E731
    for path in plan(countries, names('tags'), names('languages')):
        items = get_json(path, get)
        if items:
            out.append((path, json.dumps(trimmed(items, STATION_KEYS), ensure_ascii=False).encode('utf-8')))
        sleep(0.2)   # (the catalogue's servers are volunteers')
    return out


def pack(answers):
    """The file's bytes (MAGIC, then path, length, JSON for each)."""
    parts = [MAGIC]
    for path, body in answers:
        parts.append(path.encode('utf-8') + b'\n' + str(len(body)).encode() + b'\n' + body)
    return b''.join(parts)


def main(argv):
    answers = build()
    data = pack(answers)
    with open(argv[0], 'wb') as f:
        f.write(data)
    print('radio mirror: %d answers, %.1f MB' % (len(answers), len(data) / 1e6))
    return 0 if len(answers) > 3 else 1   # (the lists alone: radio-browser did not answer - no release)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
