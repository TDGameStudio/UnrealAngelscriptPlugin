#!/bin/bash

# AngelScriptSDK Private 命名空间批量重构脚本
#
# 此脚本自动化处理以下任务：
# 1. 将 Private 命名空间改为匿名命名空间
# 2. 移除 "using namespace XXX_Private;" 声明
# 3. 保持文件内部的全局变量和辅助函数
#
# 使用方法：
#   cd Plugins/Angelscript/Source/AngelscriptTest/AngelScriptSDK
#   bash refactor_private_namespaces.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== AngelScriptSDK Private 命名空间重构脚本 ==="
echo "工作目录: $(pwd)"
echo ""

# 统计需要处理的文件
FILES_TO_PROCESS=$(grep -l "namespace.*_Private" *.cpp | wc -l)
echo "找到 $FILES_TO_PROCESS 个需要重构的文件"
echo ""

# 备份计数器
PROCESSED=0
SKIPPED=0

# 处理每个文件
for file in *.cpp; do
    if ! grep -q "namespace.*_Private" "$file"; then
        continue
    fi

    echo "处理: $file"

    # 创建临时文件
    TEMP_FILE="${file}.tmp"

    # 步骤 1: 将命名空间定义改为匿名命名空间
    # 匹配: namespace AngelscriptTest_XXX_Private
    # 替换: namespace
    sed 's/^namespace [A-Za-z_]*_Private$/namespace/g' "$file" > "$TEMP_FILE"

    # 步骤 2: 移除 using namespace XXX_Private; 声明
    sed -i '/using namespace [A-Za-z_]*_Private;/d' "$TEMP_FILE"

    # 步骤 3: 检查是否有实际变化
    if diff -q "$file" "$TEMP_FILE" > /dev/null 2>&1; then
        echo "  → 跳过 (无变化)"
        rm "$TEMP_FILE"
        SKIPPED=$((SKIPPED + 1))
    else
        # 应用变化
        mv "$TEMP_FILE" "$file"
        echo "  ✓ 已重构"
        PROCESSED=$((PROCESSED + 1))
    fi
done

echo ""
echo "=== 重构完成 ==="
echo "处理文件: $PROCESSED"
echo "跳过文件: $SKIPPED"
echo ""
echo "下一步："
echo "1. 检查修改: git diff"
echo "2. 编译测试: 运行 RunBuild.ps1"
echo "3. 运行测试: 确保所有测试通过"
echo "4. 提交更改: git add . && git commit"
