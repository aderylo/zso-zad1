from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import TypeVar, Iterable, Callable, Any, Iterator, Optional

ROOT_PATH = Path(os.environ.get('PEX', sys.argv[0])).parent
HACKY_RELPATH: Optional[Path] = None
def GitPath(path='') -> Path:
    """Hacky path constructor that reinterprets absolute paths, which are relative to the git repo.

    When relative paths are created, their base may be overridden with global HACKY_RELPATH.
    """
    if isinstance(path, Path):
        return path

    path = Path(path)
    if path.is_absolute():
        path = ROOT_PATH / path.relative_to(path.anchor)
    elif HACKY_RELPATH:
        path = HACKY_RELPATH / path
    return path

def _set_relative_base(path: Path):
    global HACKY_RELPATH
    HACKY_RELPATH = path

TV = TypeVar('TV')

def merge_sorted(it1: Iterable[TV], it2: Iterable[TV], key: Callable[[TV,], Any] = lambda x:x) -> Iterator[TV]:
    """A helper function like ``zip``, but works on sorted (according to ``key``) iterators
    and yields a pair only if the key match. Otherwise, it yields None on one side.

    ``key(value)`` should never return ``None``.
    """
    it1, it2 = iter(it1), iter(it2)
    kl = None
    kr = None
    while True:
        if kr is None:
            try:
                r = next(it2)
                kr = key(r)
            except StopIteration:
                if kl is not None:
                    yield l, None
                yield from ((l, None) for l in it1)
                return

        if kl is None:
            try:
                l = next(it1)
                kl = key(l)
            except StopIteration:
                yield None, r
                yield from  ((None, r) for r in it2)
                return

        if kl == kr:
            yield l, r
            kl, kr = None, None
        elif kl < kr:
            yield l, None
            kl = None
        else:
            yield None, r
            kr = None
