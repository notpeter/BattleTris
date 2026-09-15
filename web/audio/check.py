#!/usr/bin/env python3
"""Validate the audio inventory against the native weapon catalog."""
import json
from pathlib import Path
import re
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def text(value):
    return isinstance(value, str) and bool(value.strip())


def relative_path(value):
    return (text(value) and not value.startswith('/') and '\\' not in value
            and ':' not in value and '..' not in value.split('/'))


def fields(value, required, optional=()):
    require(isinstance(value, dict), 'Expected an object')
    require(set(required) <= value.keys(), 'Missing fields: ' + str(set(required) - value.keys()))
    require(value.keys() <= set(required) | set(optional), 'Unexpected object fields')


def validate(manifest, root):
    fields(manifest, ['schema_version', 'status', 'native_asset_root', 'asset_url_base',
                      'cues', 'weapons', 'launch_rules', 'native_silent_events'])
    require(type(manifest['schema_version']) is int and manifest['schema_version'] == 1,
            'Unsupported schema version')
    require(text(manifest['status']), 'Invalid inventory status')
    for key in ['native_asset_root', 'asset_url_base']:
        require(relative_path(manifest[key]), 'Invalid path: ' + key)
    require(isinstance(manifest['cues'], list) and manifest['cues'], 'Missing cues')
    ids = set()
    for cue in manifest['cues']:
        fields(cue, ['id', 'assets', 'native'])
        require(text(cue['id']) and re.fullmatch(r'[a-z][a-z0-9_.]*', cue['id']),
                'Invalid cue ID')
        require(cue['id'] not in ids, 'Duplicate cue ID: ' + cue['id'])
        ids.add(cue['id'])
        require(isinstance(cue['assets'], list), 'Assets must be a list')
        for asset in cue['assets']:
            fields(asset, ['src', 'type', 'credit'])
            require(relative_path(asset['src']), 'Invalid asset path')
            require(text(asset['type']) and asset['type'].startswith('audio/'),
                    'Invalid asset media type')
            require(text(asset['credit']), 'Missing asset credit')
        native = cue['native']
        fields(native, ['source', 'trigger', 'paths', 'selection'], ['notes'])
        require(relative_path(native['source']) and (root / native['source']).is_file(),
                'Missing native source')
        require(text(native['trigger']), 'Missing native trigger')
        require(isinstance(native['paths'], list) and native['paths']
                and all(relative_path(path) for path in native['paths']),
                'Invalid native paths')
        require(native['selection'] in ['single', 'random_pool', 'resource_variant'],
                'Invalid native selection')
        require('notes' not in native or text(native['notes']), 'Invalid native notes')

    protocol = (root / 'usr/src/game/BTProtocol.H').read_text()
    enum = protocol.split('enum BTWeaponToken {', 1)[1].split('BT_MAX_WEAPONS', 1)[0]
    enum = re.sub(r'//[^\n]*', '', enum)
    tokens = re.findall(r'\bBT_[A-Z_]+\b', enum)
    records = [line.strip() for line in (root / 'usr/src/share/btweapons.db').read_text().splitlines()
               if line.strip() and not line.lstrip().startswith('#')]
    require(len(records) == 2 * len(tokens), 'Native catalog and protocol disagree')
    require(isinstance(manifest['weapons'], list)
            and len(manifest['weapons']) == len(tokens), 'Incomplete weapon mappings')
    for number, weapon in enumerate(manifest['weapons']):
        fields(weapon, ['token', 'number', 'name', 'activation_cue', 'expiration_cue',
                        'launch_cue', 'notes'])
        require(weapon['token'] == tokens[number] and type(weapon['number']) is int
                and weapon['number'] == number, 'Incorrect weapon token/order')
        require(weapon['name'] == records[number * 2], 'Incorrect weapon name')
        require(text(weapon['notes']), 'Missing weapon notes')
        for key in ['activation_cue', 'expiration_cue', 'launch_cue']:
            value = weapon[key]
            require(value is None or (text(value) and value in ids)
                    or (key == 'launch_cue' and value == 'price_based'),
                    'Dangling weapon cue: ' + str(value))
    require(isinstance(manifest['launch_rules'], list) and manifest['launch_rules'],
            'Missing launch rules')
    for rule in manifest['launch_rules']:
        fields(rule, ['when', 'cue'])
        require(text(rule['when']), 'Missing launch condition')
        require(rule['cue'] is None or (text(rule['cue']) and rule['cue'] in ids),
                'Dangling launch cue')
    require(isinstance(manifest['native_silent_events'], list)
            and all(text(event) for event in manifest['native_silent_events']),
            'Invalid silent event list')
    return len(ids), len(tokens)


if __name__ == '__main__':
    try:
        directory = Path(__file__).resolve().parent
        counts = validate(json.loads((directory / 'manifest.json').read_text()),
                          directory.parent.parent)
        print('Audio inventory: %d cues and %d weapon mappings validated.' % counts)
    except (ValueError, KeyError, IndexError, OSError, TypeError) as error:
        print('Audio inventory error: ' + str(error), file=sys.stderr)
        sys.exit(1)
