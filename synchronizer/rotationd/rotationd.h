#ifndef _ACCELERATIOND_H
#define _ACCELERATIOND_H

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct dev_rotation {
	int x; /* Angular speed around X-axis */
	int y; /* Angular speed around Y-axis */
	int z; /* Angular speed around Z-axis */
};

/**
 * struct rotation_range - Structure for user to pass the rotation event threshold.
 * @x, y, z: The angular speed around each axis in rad/(10s).
 * @samples: Number of samples needed with required rotational speed
 * @window: Max window of samples considered.
 * @bidirection: Whether allows the rotation to be bidirection around 3 axes.
 */
struct rotation_motion {
	int x;
	int y;
	int z;
	bool bidirection;
};

#endif
