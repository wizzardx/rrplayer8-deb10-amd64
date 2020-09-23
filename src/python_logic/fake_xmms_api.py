#!/var/lib/rrplayer8/venv/bin/python3.5 -u

from copy import copy
from logging import exception as log_exception
from pprint import pprint as pp

from xmlrpc.server import SimpleXMLRPCServer

from logging import DEBUG, StreamHandler, getLogger, Formatter, INFO, WARN
from logging import debug as log_debug
from logging import info as log_info
from logging import warn as log_warn
from logging.handlers import RotatingFileHandler

from os.path import isfile, splitext, exists, basename, islink
from os import symlink, remove, readlink
from time import sleep
from subprocess import check_output

import re

from typing import cast, Any

from mpd import MPDClient


def is_even(n):
    return n % 2 == 0

def sample_add(a, b):
    return a + b


class get_mpd_client:
    def __init__(self, session: int) -> None:
        assert session in [0, 1]
        self.client = MPDClient()
        self.client.connect("localhost", 6601 + session)

    def __enter__(self):
        return self.client

    def __exit__(self, *args):
        self.client.close()
        self.client.disconnect()


def fake_xmms_remote_is_playing(session: int) -> bool:
    try:
        with get_mpd_client(session) as client:
            status = client.status()
            state = status['state']
            assert state in ['play', 'stop']
            return state == 'play'
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_is_repeat(session: int) -> bool:
    try:
        with get_mpd_client(session) as client:
            status = client.status()
            repeat = status['repeat']
            assert repeat in ['0', '1']
            return repeat == '1'
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_get_main_volume(session: int) -> int:
    try:
        with get_mpd_client(session) as client:
            status = client.status()
            volume = int(status['volume'])
            assert 0 <= volume <= 100
            return volume
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_get_playlist_length(session: int) -> int:
    try:
        with get_mpd_client(session) as client:
            info = client.playlistinfo()
            return len(info)
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_get_output_time(session: int) -> int:
    try:
        with get_mpd_client(session) as client:
            # Result needs to be in milliseconds, not seconds
            status = client.status()
            if not 'elapsed' in status:
                return -999
            elapsed = float(status['elapsed'])
            ms = int(elapsed * 1000)
            return ms
    except Exception:
        log_exception('Error...')
        raise


def convert_soxi_durations_to_seconds(s: str) -> float:
    regex = '^(\d\d)\\:(\d\d)\\:(\d\d)\\.(\d\d)$'
    matches = re.findall(regex, s)
    assert len(matches) == 1
    match = matches[0]
    assert len(match) == 4

    hours = int(match[0])
    minutes = int(match[1])
    seconds = int(match[2]) + int(match[3]) / 100

    result = hours * 60 * 60 + minutes * 60 + seconds
    return result


def fake_xmms_remote_get_current_song_length_ms(session: int) -> int:
    try:
        # Result needs to be in milliseconds, not seconds
        audio_link_path = '/var/lib/rrplayer8/mpd/%d/music/audio.mp3' % (session + 1)
        if not isfile(audio_link_path):
            return -9999
        log_debug('Running soxi to determine song length...')
        output = check_output(['soxi', audio_link_path]).decode()
        duration_str = None
        for line in output.split('\n'):
            if line.startswith('Duration'):
                duration_str = line.split()[2]
                break
        assert duration_str is not None
        length_ms = int(convert_soxi_durations_to_seconds(duration_str) * 1000)
        return length_ms
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_get_current_song_title(session: int) -> str:
    try:
        with get_mpd_client(session) as client:
            # We actually return <artist> - <title>, rather than just <title>
            song_info = client.currentsong()
            artist = song_info.get('artist', '<no artist>')

            # In some weird cases, the returned artist is a list of strings, rather than just a string:
            if isinstance(artist, list):
                artist = artist[0]
            assert isinstance(artist, str), repr(artist)

            title = song_info.get('title', '<no title>')

            # In some weird cases, the returned title is a list of strings, rather than just a string:
            if isinstance(title, list):
                title = title[0]
            assert isinstance(title, str), repr(title)

            result = artist + " - " + title
            return result

    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_get_current_song_path(session: int) -> str:
    try:
        # Quick sanity check:
        with get_mpd_client(session) as client:
            songinfo = client.currentsong()
        if not 'file' in songinfo:
            return '<no song is currently playing>'
        assert songinfo['file'] == 'audio.mp3'

        # Next, get path to out audio file (actually a symlink):
        audio_link = '/var/lib/rrplayer8/mpd/%d/music/audio.mp3' % (session + 1)
        assert islink(audio_link)

        # Get destination of the symlink:
        link_dest = readlink(audio_link)

        # Check that it's a file:
        assert isfile(link_dest)

        # Return that path:
        return link_dest

    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_is_paused(session: int) -> bool:
    try:
        with get_mpd_client(session) as client:
            status = client.status()
            state = status['state']
            assert state in {'play', 'pause', 'stop'}
            return state == 'pause'
    except Exception:
        log_exception('Error...')
        raise

def fake_xmms_remote_is_running(session: int) -> bool:
    try:
        with get_mpd_client(session) as client:
            status = client.status()
            return True
    except ConnectionRefusedError:
        return False
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_stop(session: int) -> None:
    try:
        with get_mpd_client(session) as client:
            client.stop()
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_playlist_clear(session: int) -> None:
    try:
        with get_mpd_client(session) as client:
            client.clear()
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_set_main_volume(session: int, volume: int) -> None:
    try:
        assert 0 <= volume <= 100
        with get_mpd_client(session) as client:
            client.setvol(volume)
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_play(session: int) -> None:
    try:
        with get_mpd_client(session) as client:
            client.play()
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_jump_to_time(session: int, milliseconds: int) -> None:
    try:
        with get_mpd_client(session) as client:
            client.seekcur(milliseconds / 1000)
    except Exception:
        log_exception('Error...')
        raise


def fake_xmms_remote_playlist_add_url_string(session: int, url: str) -> None:
    try:
        # Check that the provided URL, is in fact an existing file path:
        assert isfile(url)

        # Get path to location (inside the music lib) that we'd want to make a
        # symlink at, pointing to the provided URL/file.
        #
        mpd_num = session + 1
        assert mpd_num in [1, 2]
        lower_ext = splitext(url)[1].lower()
        audio_file_link = '/var/lib/rrplayer8/mpd/%d/music/%s' % (mpd_num, 'audio' + lower_ext)

        # Remove symlink if present:
        if islink(audio_file_link):
            remove(audio_file_link)

        # Create symlink:
        symlink(url, audio_file_link)

        # Get basename for the audio file link:
        audio_basename = basename(audio_file_link)

        # Do some logic over the MPD API:
        with get_mpd_client(session) as client:
            # Update the mpd music library details for this file:
            client.update(audio_basename)

            # Wait for the mpd status to no longer display that it's updating
            # any music ID:
            while True:
                status = client.status()
                if 'updating_db' in status:
                    log_debug("MPD is busy updating it's database, waiting...")
                    sleep(0.2)
                else:
                    # MPD is not busy updating it's database
                    break

            # Add the new path to the MPD playlist:
            client.add(audio_basename)

    except Exception:
        log_exception('Error...')
        raise


def setup_logging(loglevel: int, logfile: str) -> None:
    # pre: n/a

    # body:
    logfmt = '[%(asctime)s] (%(process)d) - {%(threadName)s} - %(name)s - %(levelname)s: %(message)s'

    handlers = [
        StreamHandler(),
        RotatingFileHandler(logfile, "a", 50 * 1024 * 1024, 2)]

    root = getLogger()
    root.setLevel(loglevel)
    for handler in handlers:
        formatter = Formatter(logfmt)
        handler.setFormatter(formatter)
        root.addHandler(handler)

    # Also, a sneaky hack: Make the logging module use enhanced traceback-logging logic:
    class FakeTracebackModule:
        def print_exception(self, *args, **kwargs):
            from traceback2 import print_exception
            kwargs2 = copy(kwargs)
            kwargs2['show_locals'] = 100000
            return print_exception(*args, **kwargs2)
    import logging
    logging.traceback = cast(Any, FakeTracebackModule())

    # post: n/a


def main() -> None:
    setup_logging(loglevel=DEBUG, logfile='/var/log/rrplayer8/fake_xmms_api.log')

    server = SimpleXMLRPCServer(("localhost", 30126), allow_none=True)
    log_info("Listening on port 30126...")

    server.register_function(fake_xmms_remote_is_playing, "fake_xmms_remote_is_playing")
    server.register_function(fake_xmms_remote_is_running, "fake_xmms_remote_is_running")
    server.register_function(fake_xmms_remote_stop, "fake_xmms_remote_stop")
    server.register_function(fake_xmms_remote_is_repeat, "fake_xmms_remote_is_repeat")
    server.register_function(fake_xmms_remote_playlist_clear, "fake_xmms_remote_playlist_clear")
    server.register_function(fake_xmms_remote_playlist_add_url_string, "fake_xmms_remote_playlist_add_url_string")
    server.register_function(fake_xmms_remote_get_main_volume, "fake_xmms_remote_get_main_volume")
    server.register_function(fake_xmms_remote_set_main_volume, "fake_xmms_remote_set_main_volume")
    server.register_function(fake_xmms_remote_get_playlist_length, "fake_xmms_remote_get_playlist_length")
    server.register_function(fake_xmms_remote_play, "fake_xmms_remote_play")
    server.register_function(fake_xmms_remote_is_paused, "fake_xmms_remote_is_paused")
    server.register_function(fake_xmms_remote_get_output_time, "fake_xmms_remote_get_output_time")
    server.register_function(fake_xmms_remote_get_current_song_length_ms, "fake_xmms_remote_get_current_song_length_ms")
    server.register_function(fake_xmms_remote_get_current_song_title, "fake_xmms_remote_get_current_song_title")
    server.register_function(fake_xmms_remote_get_current_song_path, "fake_xmms_remote_get_current_song_path")
    server.register_function(fake_xmms_remote_jump_to_time, "fake_xmms_remote_jump_to_time")

    server.serve_forever()

if __name__ == '__main__':
    main()
