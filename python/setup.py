#!/usr/bin/env python3
"""
Setup script for RT-STT Python client
"""

from setuptools import setup, find_packages
import os

# Read README for long description
here = os.path.abspath(os.path.dirname(__file__))
readme_path = os.path.join(os.path.dirname(here), 'README.md')

long_description = ""
if os.path.exists(readme_path):
    with open(readme_path, encoding='utf-8') as f:
        long_description = f.read()

setup(
    name='rt-stt',
    version='1.0.0',
    description='Real-time Speech-to-Text client for macOS',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='RT-STT Team',
    url='https://github.com/yourusername/rt-stt',
    packages=find_packages(),
    python_requires='>=3.7',
    install_requires=[
        # No external dependencies - uses only stdlib!
    ],
    extras_require={
        'dev': [
            'pytest>=6.0',
            'pytest-cov',
            'black',
            'flake8',
            'mypy',
        ],
    },
    entry_points={
        'console_scripts': [
            'rt-stt-cli=rt_stt.cli:main',
            'rt-stt-client=rt_stt.__main__:main',
        ],
    },
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'Topic :: Multimedia :: Sound/Audio :: Speech',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Operating System :: MacOS',
    ],
    keywords='speech-to-text stt whisper macos realtime',
)
