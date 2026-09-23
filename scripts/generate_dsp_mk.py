#!/usr/bin/env python3
import os, glob

def gen_subdir(folder_name):
    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    src_dir = os.path.join(base_dir, 'Drivers', 'CMSIS_DSP', 'Source', folder_name)
    debug_dir = os.path.join(base_dir, 'Debug', 'Drivers', 'CMSIS_DSP', 'Source', folder_name)
    os.makedirs(debug_dir, exist_ok=True)

    c_files = sorted(glob.glob(os.path.join(src_dir, '*.c')))
    basenames = [os.path.basename(f) for f in c_files]

    c_srcs = [f'../Drivers/CMSIS_DSP/Source/{folder_name}/{b}' for b in basenames]
    objs = [f'./Drivers/CMSIS_DSP/Source/{folder_name}/{b[:-2]}.o' for b in basenames]
    deps = [f'./Drivers/CMSIS_DSP/Source/{folder_name}/{b[:-2]}.d' for b in basenames]

    mk_path = os.path.join(debug_dir, 'subdir.mk')
    with open(mk_path, 'w') as f:
        f.write('################################################################################\n')
        f.write('# Automatically-generated file. Do not edit!\n')
        f.write('################################################################################\n\n')
        f.write('C_SRCS += \\\n' + ' \\\n'.join(c_srcs) + ' \n\n')
        f.write('OBJS += \\\n' + ' \\\n'.join(objs) + ' \n\n')
        f.write('C_DEPS += \\\n' + ' \\\n'.join(deps) + ' \n\n')
        f.write(f'Drivers/CMSIS_DSP/Source/{folder_name}/%.o Drivers/CMSIS_DSP/Source/{folder_name}/%.su Drivers/CMSIS_DSP/Source/{folder_name}/%.cyclo: ../Drivers/CMSIS_DSP/Source/{folder_name}/%.c Drivers/CMSIS_DSP/Source/{folder_name}/subdir.mk\n')
        f.write('\tarm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -DARM_MATH_CM4 -D__FPU_PRESENT=1 -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Drivers/CMSIS_DSP/Include -I../Drivers/CMSIS_DSP/DSP_Include -I../Drivers/CMSIS_DSP/PrivateInclude -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"\n\n')
        f.write(f'clean: clean-Drivers-2f-CMSIS_DSP-2f-Source-2f-{folder_name}\n\n')
        f.write(f'clean-Drivers-2f-CMSIS_DSP-2f-Source-2f-{folder_name}:\n\t-$(RM) ' + ' '.join([f'./Drivers/CMSIS_DSP/Source/{folder_name}/{b[:-2]}.o' for b in basenames]) + '\n\n')
        f.write(f'.PHONY: clean-Drivers-2f-CMSIS_DSP-2f-Source-2f-{folder_name}\n')

    # Add objects to objects.list if not already present
    obj_list_path = os.path.join(base_dir, 'Debug', 'objects.list')
    with open(obj_list_path, 'r') as f:
        existing = f.read()

    new_objs = []
    for o in objs:
        quoted = f'"{o}"'
        if quoted not in existing:
            new_objs.append(quoted)

    if new_objs:
        with open(obj_list_path, 'a') as f:
            for no in new_objs:
                f.write(no + '\n')

    # Add to makefile includes if not present
    makefile_path = os.path.join(base_dir, 'Debug', 'makefile')
    with open(makefile_path, 'r') as f:
        mk_content = f.read()

    inc_str = f'-include Drivers/CMSIS_DSP/Source/{folder_name}/subdir.mk'
    if inc_str not in mk_content:
        mk_content = mk_content.replace('-include objects.mk', f'{inc_str}\n-include objects.mk')
        with open(makefile_path, 'w') as f:
            f.write(mk_content)

if __name__ == '__main__':
    gen_subdir('TransformFunctions')
    gen_subdir('BasicMathFunctions')
    print("CMSIS-DSP TransformFunctions & BasicMathFunctions generated and added to makefile & objects.list successfully!")
