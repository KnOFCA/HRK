"""Prepare a fresh checkout without putting signing material into version control."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[1] / 'src/app/harmony'
profile = root / 'build-profile.json5'
if not profile.exists():
    shutil.copyfile(root / 'build-profile.template.json5', profile)
print('Harmony project ready; existing local signing settings preserved.')
