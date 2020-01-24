all: ./interval-split-scaling.csv ./interval-split-scaling.png

./interval-split-scaling-raw.csv: ./interval-split-scaling.R ./data.csv
	cd ../experiments && ./runner.py list | xsv search -s net 20x20 |xsv search -s jobs '4|8|16|32|64|128|256|512' |xsv search -s initial_timeout 80| xsv sort -s jobs -N| xsv sort -s prop > ../analysis/$@ && cd -

./interval-split-scaling.csv: ./interval-split-scaling.R ./interval-split-scaling-raw.csv
	R --no-save < $<

./interval-split-scaling.png: ./interval-split-scaling.R ./interval-split-scaling-raw.csv
	R --no-save < $<

clean:
	rm -f ./*.png
