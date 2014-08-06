#ifndef _EDT_VTL_CT36X_H
#define _EDT_VTL_CT36X_H

/*
 * Copyright (c) 2012 Simon Budig, <simon.budig@kernelconcepts.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

struct ts_config_info{      
        
        unsigned int	screen_max_x;
        unsigned int	screen_max_y;
	unsigned int	irq_gpio_number;
	unsigned int	irq_number;
        unsigned int	rst_gpio_number;
	unsigned char	touch_point_number;
	unsigned char	ctp_used;
};

#endif /* _EDT_VTL_CT36X_H */
