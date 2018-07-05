#include <thread>
#include <Windows.h>
//#include <unistd.h>
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "dots.h"
#include "tile.h"
#include "time.h"


//Ref: https://gist.github.com/ngryman/6482577
void usleep(unsigned int usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * (__int64)usec);

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}



class CSVRow
{
    public:
        std::string const& operator[](std::size_t index) const
        {
            return m_data[index];
        }
        std::size_t size() const
        {
            return m_data.size();
        }
        void readNextRow(std::istream& str)
        {
            std::string         line;
            std::getline(str, line);

            std::stringstream   lineStream(line);
            std::string         cell;

            m_data.clear();
            while(std::getline(lineStream, cell, ','))
            {
                m_data.push_back(cell);
            }
            // This checks for a trailing comma with no data after it.
            if (!lineStream && cell.empty())
            {
                // If there was a trailing comma then add an empty element.
                m_data.push_back("");
            }
        }
    private:
        std::vector<std::string>    m_data;
};

std::istream& operator>>(std::istream& str, CSVRow& data)
{
    data.readNextRow(str);
    return str;
}

bool operator<(Point o1, Point o2) {
	return o1.lng < o2.lng || (o1.lng == o2.lng && o1.lat < o2.lat);
}

bool operator==(Point o1, Point o2) {
	return o1.lng == o2.lng && o1.lat == o2.lat;
}

void Dots::run() {
	time_start = get_time();

	std::thread t([this]() {
		std::ifstream       file("data.csv");

		uint64_t base = 0;
		CSVRow row;
		while(file >> row)
		{
			uint64_t ts = std::stoull(row[1]);
			double lng = std::stod(row[2]);
			double lat = std::stod(row[3]);

			Point point{lng, lat};
			auto item = dots.find(point);
			if (item != dots.end()) {
				item->second.size++;
			} else {
				dots[point] = make_dot(lng, lat, ts);
			}

			if (time_base > 0) {
				usleep((ts - base) / (1000 * speed));
			} else {
				time_base = ts;
			}

			base = ts;
		}
	});

	t.detach();
}

Dot Dots::make_dot(double lng, double lat, uint64_t seen_at) {
	Dot dot = Dot{lng, lat, seen_at, 1, {}, {}};

	for (int i = MIN_ZOOM; i <= MAX_ZOOM; i++) {
		int map_size = mapsize(i);
		dot.zoom_x[i] = long2x(lng, map_size);
		dot.zoom_y[i] = lat2y(lat, map_size);
	}

	return dot;
}

uint64_t Dots::time_diff(uint64_t seen_at, uint64_t current) {
	if (seen_at < time_base) {
		return 0;
	}

	if (current < time_start) {
		return 0;
	}

	uint64_t be = seen_at - time_base;
	uint64_t ce = (current - time_start) / speed;

	if (ce < be) {
		return 0;
	}

	return ce - be;
}
