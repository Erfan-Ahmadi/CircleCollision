#pragma once
#include "..//common.hpp"
#include <vector>

/*
* A Grid has partitions
* Approach #1(static):	Set Number of X and Y Partitions
* Approach #2(dynamic):	Partition evenly (Squares) such that minimum size is the maximum circle size
* Each Partition has index numbers of the circles in partition (not Pointers to Circle Objects).
* Algorithms are exactly like before but in executed in each partition
*/

struct grid
{
	std::vector<std::vector<size>> partition_indexes;
	std::vector<float> columns;
	std::vector<float> rows;

	void init_grid(size_t vertical_count, size_t horizontal_count)
	{
		const float width = (float)screen_width / vertical_count;
		for (size i = 0; i <= vertical_count; ++i)
		{
			columns.push_back(width * i);
		}

		const float height = (float)screen_height / horizontal_count;
		for (size i = 0; i <= horizontal_count; ++i)
		{
			rows.push_back(height * i);
		}

		partition_indexes = std::vector<std::vector<size>>(horizontal_count * vertical_count);
	}

	inline void add_to_partition(const size& partition, const size& index)
	{
		partition_indexes[partition].push_back(index);
	}

	inline size detect_partition_x(const float& x)
	{
		size lowx = 0;
		size high = columns.size() - 2;
		size mid = (lowx + high) / 2;

		while (lowx < high)
		{
			if (x < columns[mid])
			{
				high = mid - 1;
			}
			else
			{
				lowx = mid;
			}

			mid = (lowx + high + 1) / 2;
		}

		return lowx;
	}

	inline size detect_partition_y(const float& y)
	{
		size lowy = 0;
		size high = rows.size() - 2;
		size mid = (lowy + high) / 2;

		while (lowy < high)
		{
			if (y < rows[mid])
			{
				high = mid - 1;
			}
			else
			{
				lowy = mid;
			}

			mid = (lowy + high + 1) / 2;
		}

		return lowy;
	}

	inline size detect_partition(const float& x, const float& y)
	{
		return detect_partition_x(x) + detect_partition_y(y) * (columns.size() - 1);
	}

	void add_circle_to_partition(const size& index, const float& x, const float& y, const float& radius)
	{
		size right_par = detect_partition_x(x + radius);
		size left_par = detect_partition_x(x - radius);
		size bottom_par = detect_partition_y(y - radius);
		size top_par = detect_partition_y(y + radius);

		for(size i = left_par; i <= right_par; ++i)
			for(size j = bottom_par; j <= top_par; ++j)
				add_to_partition(i + j * (columns.size() - 1), index);
	}

	inline void add_to_partition(const size& index, const float& x, const float& y)
	{
		add_to_partition(detect_partition(x, y), index);
	}

	void clear_partitions()
	{
		for (auto& p : partition_indexes)
			p.clear();
	}

	//needs re-init
	void clear_grid()
	{
		partition_indexes.clear();
		columns.clear();
		rows.clear();
	}
};