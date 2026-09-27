import sys, os
from PIL import Image
prefix, out = sys.argv[1], sys.argv[2]
ns = [n for n in sys.argv[3:] if os.path.exists('out/%s_%s.ppm' % (prefix, n))]
ims = [Image.open('out/%s_%s.ppm' % (prefix, n)).resize((480, 360), Image.NEAREST) for n in ns]
cols = 2
rows = (len(ims) + 1) // 2
s = Image.new('RGB', (cols * 490, rows * 370))
for i, im in enumerate(ims): s.paste(im, ((i % cols) * 490, (i // cols) * 370))
s.save(out); print(ns)
