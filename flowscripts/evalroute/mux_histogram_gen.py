import xlsxwriter
import sys

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log

def generate_excel(mux_histogram_file, excel_name):
    # Open the input file and read in the lines
    with open(mux_histogram_file, 'r') as file:
        lines = file.readlines()

    # Create a new workbook and worksheet
    workbook = xlsxwriter.Workbook(f'{excel_name}.xlsx')
    worksheet = workbook.add_worksheet()

    # Define some formatting for the headers and cells
    bold = workbook.add_format({'bold': True})
    cell_format = workbook.add_format({'align': 'center'})

    # Write the headers to the first row of the worksheet
    # worksheet.write('A1', 'Coordinate', bold)
    # worksheet.write('B1', '#Muxes', bold)
    # worksheet.write('C1', 'Mux Type', bold)
    # worksheet.write('D1', 'Width', bold)

    # Loop through the lines and write the data to the worksheet
    prev_x = prev_y = -1
    cur_col = cur_row = -1
    count = 1
    for i, line in enumerate(lines):
        if i < 2:
            # print(f"ignoring {line} line")
            continue

        # Split the line into its individual fields
        fields = line.strip().split()

        # Get the maximum x and y coordinates
        if i == 2:
            max_x, max_y = map(int, fields[0].strip('()').split(','))
        

        # Get the coordinates and convert them to row and column indices
        x, y = map(int, fields[0].strip('()').split(','))
        
        if prev_y == y:
            row = cur_row + count
            count += 1
        else:
            row = (max_y - y) * 10
            prev_y = y
            cur_row = row
            count = 1

        col = x * 4

        # Write the data to the appropriate cells in the worksheet
        worksheet.write(row, col, f'({x},{y})', cell_format)
        worksheet.write(row, col+1, int(fields[1]), cell_format)
        worksheet.write(row, col+2, fields[2], cell_format)
        worksheet.write(row, col+3, int(fields[3]), cell_format)

    # Define the data
    # data = [
    #     [32, 'CB', 24],
    #     [22, 'CB', 22],
    #     [72, 'SB', 8],
    #     [84, 'SB', 5]
    # ]

    # # Define the format for cells that meet the criteria
    # format_criteria = workbook.add_format({'bg_color': 'green'})
    # formula = '=AND(AND(A1=32,B1="CB",C1=24),AND(A2=22,B2="CB",C2=22),AND(A3=72,B3="SB",C3=8),AND(A4=84,B4="SB",C4=5))'
    # worksheet.conditional_format(0,0,worksheet.dim_rowmax, worksheet.dim_colmax, {'type':     'formula',
    #                                                                               'criteria': formula,
    #                                                                               'format': format_criteria})
    # for row_num in range(worksheet.dim_rowmax + 1):
    #     if worksheet.write_formula(row_num, 0, '=AND(AND(A1=32,B1="CB",C1=24),AND(A2=22,B2="CB",C2=22),AND(A3=72,B3="SB",C3=8),AND(A4=84,B4="SB",C4=5))', format_criteria):
            
    #         for col_num in range(1, 3):
    #             worksheet.set_cell(row_num, col_num, None, format_criteria)
    #         for col_num in range(1, 3):
    #             worksheet.set_cell(row_num+1, col_num, None, format_criteria)
    #         for col_num in range(1, 3):
    #             worksheet.set_cell(row_num+2, col_num, None, format_criteria)
    #         for col_num in range(1, 3):
    #             worksheet.set_cell(row_num+3, col_num, None, format_criteria)
    # Close the workbook
    workbook.close()

arguments = sys.argv
if len(arguments) > 1:
    mux_histogram_file = arguments[1]
    excel_name = mux_histogram_file.split('/')[-1]
    generate_excel(mux_histogram_file, excel_name)
else:
    log("bad arguments\n")