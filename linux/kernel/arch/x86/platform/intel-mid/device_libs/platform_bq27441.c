/*s software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *              
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */             
#include <linux/input.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>


static struct i2c_board_info __initdata bq27x00_i2c_device = {
       I2C_BOARD_INFO("bq27441", 0x55),
};  

static int __init bq27441_i2c_init(void)
{
   	return	i2c_register_board_info(1, &bq27x00_i2c_device, 1);
}

module_init(bq27441_i2c_init);

MODULE_AUTHOR("Wenzeng Chen <wenzeng.chen@intel.com>");
MODULE_DESCRIPTION("FT5x06 I2C Touchscreen Platform Driver");
MODULE_LICENSE("GPL");

