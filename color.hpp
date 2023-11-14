#pragma once

struct Color {
  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;

  // https://github.com/mbadolato/iTerm2-Color-Schemes/blob/master/windowsterminal/Ubuntu.json
  static Color from8(unsigned char val) {
    switch (val) {
      case 0:
        return {46, 52, 54};
      case 1:
        return {204, 0, 0};
      case 2:
        return {78, 154, 6};
      case 3:
        return {196, 160, 0};
      case 4:
        return {52, 101, 164};
      case 5:
        return {117, 80, 123};
      case 6:
        return {6, 152, 154};
      default:
        return {211, 215, 207};
    }
  }

  static Color from8bright(unsigned char val) {
    switch (val) {
      case 0:
        return {85, 85, 83};
      case 1:
        return {239, 41, 41};
      case 2:
        return {138, 226, 52};
      case 3:
        return {252, 233, 79};
      case 4:
        return {114, 159, 207};
      case 5:
        return {173, 127, 168};
      case 6:
        return {52, 226, 226};
      default:
        return {238, 238, 236};
    }
  }

  // https://www.ditig.com/256-colors-cheat-sheet
  static Color from256(unsigned char val) {
    switch (val) {
      case 0:
        return {0, 0, 0};
      case 1:
        return {128, 0, 0};
      case 2:
        return {0, 128, 0};
      case 3:
        return {128, 128, 0};
      case 4:
        return {0, 0, 128};
      case 5:
        return {128, 0, 128};
      case 6:
        return {0, 128, 128};
      case 7:
        return {192, 192, 192};
      case 8:
        return {128, 128, 128};
      case 9:
        return {255, 0, 0};
      case 10:
        return {0, 255, 0};
      case 11:
        return {255, 255, 0};
      case 12:
        return {0, 0, 255};
      case 13:
        return {255, 0, 255};
      case 14:
        return {0, 255, 255};
      case 15:
        return {255, 255, 255};
      case 16:
        return {0, 0, 0};
      case 17:
        return {0, 0, 95};
      case 18:
        return {0, 0, 135};
      case 19:
        return {0, 0, 175};
      case 20:
        return {0, 0, 215};
      case 21:
        return {0, 0, 255};
      case 22:
        return {0, 95, 0};
      case 23:
        return {0, 95, 95};
      case 24:
        return {0, 95, 135};
      case 25:
        return {0, 95, 175};
      case 26:
        return {0, 95, 215};
      case 27:
        return {0, 95, 255};
      case 28:
        return {0, 135, 0};
      case 29:
        return {0, 135, 95};
      case 30:
        return {0, 135, 135};
      case 31:
        return {0, 135, 175};
      case 32:
        return {0, 135, 215};
      case 33:
        return {0, 135, 255};
      case 34:
        return {0, 175, 0};
      case 35:
        return {0, 175, 95};
      case 36:
        return {0, 175, 135};
      case 37:
        return {0, 175, 175};
      case 38:
        return {0, 175, 215};
      case 39:
        return {0, 175, 255};
      case 40:
        return {0, 215, 0};
      case 41:
        return {0, 215, 95};
      case 42:
        return {0, 215, 135};
      case 43:
        return {0, 215, 175};
      case 44:
        return {0, 215, 215};
      case 45:
        return {0, 215, 255};
      case 46:
        return {0, 255, 0};
      case 47:
        return {0, 255, 95};
      case 48:
        return {0, 255, 135};
      case 49:
        return {0, 255, 175};
      case 50:
        return {0, 255, 215};
      case 51:
        return {0, 255, 255};
      case 52:
        return {95, 0, 0};
      case 53:
        return {95, 0, 95};
      case 54:
        return {95, 0, 135};
      case 55:
        return {95, 0, 175};
      case 56:
        return {95, 0, 215};
      case 57:
        return {95, 0, 255};
      case 58:
        return {95, 95, 0};
      case 59:
        return {95, 95, 95};
      case 60:
        return {95, 95, 135};
      case 61:
        return {95, 95, 175};
      case 62:
        return {95, 95, 215};
      case 63:
        return {95, 95, 255};
      case 64:
        return {95, 135, 0};
      case 65:
        return {95, 135, 95};
      case 66:
        return {95, 135, 135};
      case 67:
        return {95, 135, 175};
      case 68:
        return {95, 135, 215};
      case 69:
        return {95, 135, 255};
      case 70:
        return {95, 175, 0};
      case 71:
        return {95, 175, 95};
      case 72:
        return {95, 175, 135};
      case 73:
        return {95, 175, 175};
      case 74:
        return {95, 175, 215};
      case 75:
        return {95, 175, 255};
      case 76:
        return {95, 215, 0};
      case 77:
        return {95, 215, 95};
      case 78:
        return {95, 215, 135};
      case 79:
        return {95, 215, 175};
      case 80:
        return {95, 215, 215};
      case 81:
        return {95, 215, 255};
      case 82:
        return {95, 255, 0};
      case 83:
        return {95, 255, 95};
      case 84:
        return {95, 255, 135};
      case 85:
        return {95, 255, 175};
      case 86:
        return {95, 255, 215};
      case 87:
        return {95, 255, 255};
      case 88:
        return {135, 0, 0};
      case 89:
        return {135, 0, 95};
      case 90:
        return {135, 0, 135};
      case 91:
        return {135, 0, 175};
      case 92:
        return {135, 0, 215};
      case 93:
        return {135, 0, 255};
      case 94:
        return {135, 95, 0};
      case 95:
        return {135, 95, 95};
      case 96:
        return {135, 95, 135};
      case 97:
        return {135, 95, 175};
      case 98:
        return {135, 95, 215};
      case 99:
        return {135, 95, 255};
      case 100:
        return {135, 135, 0};
      case 101:
        return {135, 135, 95};
      case 102:
        return {135, 135, 135};
      case 103:
        return {135, 135, 175};
      case 104:
        return {135, 135, 215};
      case 105:
        return {135, 135, 255};
      case 106:
        return {135, 175, 0};
      case 107:
        return {135, 175, 95};
      case 108:
        return {135, 175, 135};
      case 109:
        return {135, 175, 175};
      case 110:
        return {135, 175, 215};
      case 111:
        return {135, 175, 255};
      case 112:
        return {135, 215, 0};
      case 113:
        return {135, 215, 95};
      case 114:
        return {135, 215, 135};
      case 115:
        return {135, 215, 175};
      case 116:
        return {135, 215, 215};
      case 117:
        return {135, 215, 255};
      case 118:
        return {135, 255, 0};
      case 119:
        return {135, 255, 95};
      case 120:
        return {135, 255, 135};
      case 121:
        return {135, 255, 175};
      case 122:
        return {135, 255, 215};
      case 123:
        return {135, 255, 255};
      case 124:
        return {175, 0, 0};
      case 125:
        return {175, 0, 95};
      case 126:
        return {175, 0, 135};
      case 127:
        return {175, 0, 175};
      case 128:
        return {175, 0, 215};
      case 129:
        return {175, 0, 255};
      case 130:
        return {175, 95, 0};
      case 131:
        return {175, 95, 95};
      case 132:
        return {175, 95, 135};
      case 133:
        return {175, 95, 175};
      case 134:
        return {175, 95, 215};
      case 135:
        return {175, 95, 255};
      case 136:
        return {175, 135, 0};
      case 137:
        return {175, 135, 95};
      case 138:
        return {175, 135, 135};
      case 139:
        return {175, 135, 175};
      case 140:
        return {175, 135, 215};
      case 141:
        return {175, 135, 255};
      case 142:
        return {175, 175, 0};
      case 143:
        return {175, 175, 95};
      case 144:
        return {175, 175, 135};
      case 145:
        return {175, 175, 175};
      case 146:
        return {175, 175, 215};
      case 147:
        return {175, 175, 255};
      case 148:
        return {175, 215, 0};
      case 149:
        return {175, 215, 95};
      case 150:
        return {175, 215, 135};
      case 151:
        return {175, 215, 175};
      case 152:
        return {175, 215, 215};
      case 153:
        return {175, 215, 255};
      case 154:
        return {175, 255, 0};
      case 155:
        return {175, 255, 95};
      case 156:
        return {175, 255, 135};
      case 157:
        return {175, 255, 175};
      case 158:
        return {175, 255, 215};
      case 159:
        return {175, 255, 255};
      case 160:
        return {215, 0, 0};
      case 161:
        return {215, 0, 95};
      case 162:
        return {215, 0, 135};
      case 163:
        return {215, 0, 175};
      case 164:
        return {215, 0, 215};
      case 165:
        return {215, 0, 255};
      case 166:
        return {215, 95, 0};
      case 167:
        return {215, 95, 95};
      case 168:
        return {215, 95, 135};
      case 169:
        return {215, 95, 175};
      case 170:
        return {215, 95, 215};
      case 171:
        return {215, 95, 255};
      case 172:
        return {215, 135, 0};
      case 173:
        return {215, 135, 95};
      case 174:
        return {215, 135, 135};
      case 175:
        return {215, 135, 175};
      case 176:
        return {215, 135, 215};
      case 177:
        return {215, 135, 255};
      case 178:
        return {215, 175, 0};
      case 179:
        return {215, 175, 95};
      case 180:
        return {215, 175, 135};
      case 181:
        return {215, 175, 175};
      case 182:
        return {215, 175, 215};
      case 183:
        return {215, 175, 255};
      case 184:
        return {215, 215, 0};
      case 185:
        return {215, 215, 95};
      case 186:
        return {215, 215, 135};
      case 187:
        return {215, 215, 175};
      case 188:
        return {215, 215, 215};
      case 189:
        return {215, 215, 255};
      case 190:
        return {215, 255, 0};
      case 191:
        return {215, 255, 95};
      case 192:
        return {215, 255, 135};
      case 193:
        return {215, 255, 175};
      case 194:
        return {215, 255, 215};
      case 195:
        return {215, 255, 255};
      case 196:
        return {255, 0, 0};
      case 197:
        return {255, 0, 95};
      case 198:
        return {255, 0, 135};
      case 199:
        return {255, 0, 175};
      case 200:
        return {255, 0, 215};
      case 201:
        return {255, 0, 255};
      case 202:
        return {255, 95, 0};
      case 203:
        return {255, 95, 95};
      case 204:
        return {255, 95, 135};
      case 205:
        return {255, 95, 175};
      case 206:
        return {255, 95, 215};
      case 207:
        return {255, 95, 255};
      case 208:
        return {255, 135, 0};
      case 209:
        return {255, 135, 95};
      case 210:
        return {255, 135, 135};
      case 211:
        return {255, 135, 175};
      case 212:
        return {255, 135, 215};
      case 213:
        return {255, 135, 255};
      case 214:
        return {255, 175, 0};
      case 215:
        return {255, 175, 95};
      case 216:
        return {255, 175, 135};
      case 217:
        return {255, 175, 175};
      case 218:
        return {255, 175, 215};
      case 219:
        return {255, 175, 255};
      case 220:
        return {255, 215, 0};
      case 221:
        return {255, 215, 95};
      case 222:
        return {255, 215, 135};
      case 223:
        return {255, 215, 175};
      case 224:
        return {255, 215, 215};
      case 225:
        return {255, 215, 255};
      case 226:
        return {255, 255, 0};
      case 227:
        return {255, 255, 95};
      case 228:
        return {255, 255, 135};
      case 229:
        return {255, 255, 175};
      case 230:
        return {255, 255, 215};
      case 231:
        return {255, 255, 255};
      case 232:
        return {8, 8, 8};
      case 233:
        return {18, 18, 18};
      case 234:
        return {28, 28, 28};
      case 235:
        return {38, 38, 38};
      case 236:
        return {48, 48, 48};
      case 237:
        return {58, 58, 58};
      case 238:
        return {68, 68, 68};
      case 239:
        return {78, 78, 78};
      case 240:
        return {88, 88, 88};
      case 241:
        return {98, 98, 98};
      case 242:
        return {108, 108, 108};
      case 243:
        return {118, 118, 118};
      case 244:
        return {128, 128, 128};
      case 245:
        return {138, 138, 138};
      case 246:
        return {148, 148, 148};
      case 247:
        return {158, 158, 158};
      case 248:
        return {168, 168, 168};
      case 249:
        return {178, 178, 178};
      case 250:
        return {188, 188, 188};
      case 251:
        return {198, 198, 198};
      case 252:
        return {208, 208, 208};
      case 253:
        return {218, 218, 218};
      case 254:
        return {228, 228, 228};
      case 255:
        return {238, 238, 238};
    }
  }
};
