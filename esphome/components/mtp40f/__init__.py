import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@your_github_username"] # 당신의 깃허브 사용자 이름으로 변경하세요.

mtp40f_ns = cg.esphome_ns.namespace("mtp40f")
MTP40FComponent = mtp40f_ns.class_("MTP40FComponent", cg.PollingComponent, cg.Component)

# 액션 클래스 참조 (mtp40f.h에 정의된 액션 클래스들과 일치해야 합니다)
MTP40FEnableSelfCalibrationAction = mtp40f_ns.class_("MTP40FEnableSelfCalibrationAction")
MTP40FDisableSelfCalibrationAction = mtp40f_ns.class_("MTP40FDisableSelfCalibrationAction")

# 아직 C++ 파일에 구현되지 않은 액션들은 필요 시 주석 처리하거나, C++ 구현 후 추가합니다.
# MTP40FSetAirPressureReferenceAction = mtp40f_ns.class_("MTP40FSetAirPressureReferenceAction")
# MTP40FSetSelfCalibrationHoursAction = mtp40f_ns.class_("MTP40FSetSelfCalibrationHoursAction")
# MTP40FPerformSinglePointCorrectionAction = mtp40f_ns.class_("MTP40FPerformSinglePointCorrectionAction")
