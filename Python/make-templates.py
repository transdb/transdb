#!/usr/bin/env python

import sys
import argparse
import os

def main():
    """
    Main function to start build templates.py file.
    """
    usage = 'Usage: %prog src_dir output_file'
    parser = argparse.ArgumentParser()
    parser.add_argument("src_dir", help="source directory with .html files]", nargs='?', default='template_src')
    parser.add_argument("output_file", help="path to output file, default is ./templates.py", nargs='?', default='./templates.py')
    args = parser.parse_args()
    
    src_files = os.listdir(args.src_dir)
    templates = {} 
    for f in src_files :
        with open(os.path.join(args.src_dir, f), "r") as src : 
            tpl = src.read().replace('\n','')
        print(os.path.splitext(f)[0])
        templates[os.path.splitext(f)[0]] = tpl

    with open(args.output_file, "w") as out:
        out.write("templates=")
        out.write(str(templates))

if __name__ == '__main__':
    main()
    
