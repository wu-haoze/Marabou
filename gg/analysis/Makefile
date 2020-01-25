all: interval-split-scaling.csv interval-split-scaling.pdf relu-split-scaling.csv relu-split-scaling.pdf

interval-split-scaling-raw.csv: ../experiments/runner.py
	cd ../experiments && ./runner.py list |xsv search -s jobs '4|8|16|32|64|128|256|512|1024|1000' | xsv sort -s prop > ../analysis/$@ && cd -

interval-split-scaling.csv: interval-split-scaling.R interval-split-scaling-raw.csv
	R --no-save < $<

interval-split-scaling.pdf: interval-split-scaling.R interval-split-scaling-raw.csv
	R --no-save < $<

relu-split-scaling-raw.csv: ../experiments/runner.py
	cd ../experiments && ./runner.py list | xsv search -s net 20x20 |xsv search -s jobs '4|8|16|32|64|128|256|512|1024|1000' |xsv search -s initial_timeout 80| xsv sort -s jobs -N| xsv sort -s prop > ../analysis/$@ && cd -

relu-split-scaling.csv: relu-split-scaling.R relu-split-scaling-raw.csv
	R --no-save < $<

relu-split-scaling.pdf: relu-split-scaling.R relu-split-scaling-raw.csv
	R --no-save < $<

clean:
	rm -f ./*.png ./*.csv
