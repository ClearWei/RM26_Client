# 公开镜像发布手册

现有研发仓库不直接切换为公开仓库。首个公开版本必须从通过门禁的提交导出一份
无历史快照，再在新目录建立根提交。这样只发布客户端、模拟器、测试以及它们必需的构建和
说明文件，不携带研发仓库的内部历史、任务记录和本地日志。重写研发仓库历史不是本手册接受的
替代方案。

## 1. 发布负责人确认

开始导出前，需要在发布记录中写清：

- 学校、战队和主要贡献者已经确认代码权属；
- 根目录许可证与贡献方式已经确定；
- `tools/release/public_assets.json` 中进入发布包的素材均为 `approved`，并有可复核证据；
- 安全问题和社区行为问题有可用的私密联系渠道；
- 协议基线、第三方依赖和二进制许可证结果已经复核；
- 已按[开源发行与打包契约](packaging-contract.md)决定 `source` 或 `binary` 模式；
- `tools/release/packaging_policy.json` 与实际交付方式一致，不再是 `undecided`。

这些事项不能由脚本代替项目方决定。

## 2. 固定发布提交

发布提交必须没有未暂存或未跟踪的发布内容，并通过：

```bash
python3 tools/release/check_public_readiness.py
python3 tools/release/check_media_metadata.py
python3 tools/release/check_packaging_readiness.py
python3 tools/release/check_example_config.py
python3 tools/release/check_docs.py
python3 tools/release/check_runtime_resources.py
python3 tools/release/check_version_contract.py
python3 tools/release/check_sim_protobuf_runtime.py
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

媒体检查依赖 `ffprobe`。保存提交 SHA、平台、依赖版本、发布模式和测试输出。任一预检返回非零时
停止，不使用忽略退出码或临时删除检查规则绕过。当前 `release_mode=source` 只允许源码归档；
如果策略与实际交付内容不一致，打包检查必须失败。

## 3. 导出无历史快照

选择一个不存在的新目录，并从已验证提交导出：

```bash
export RM26_PUBLIC_DIR="/path/to/new-rm26-public"
test ! -e "$RM26_PUBLIC_DIR"
mkdir -p "$RM26_PUBLIC_DIR"
git archive --format=tar HEAD | tar -xf - -C "$RM26_PUBLIC_DIR"
```

`git archive` 会按 `.gitattributes` 中的 `export-ignore` 规则排除阶段性文档、工作区配置和任务记录。
导出后先复核文件清单，确认只剩下客户端、模拟器、测试、构建脚本和对外说明：

```bash
git archive --format=tar --list HEAD | LC_ALL=C sort
```

`check_docs.py` 和 `check_public_readiness.py` 检查的是 Git 索引中的发布快照。导出后先建立临时索引，再运行检查：

```bash
git -C "$RM26_PUBLIC_DIR" init -b main
git -C "$RM26_PUBLIC_DIR" add --all

cd "$RM26_PUBLIC_DIR"
python3 tools/release/check_public_readiness.py
python3 tools/release/check_media_metadata.py
python3 tools/release/check_packaging_readiness.py
python3 tools/release/check_example_config.py
python3 tools/release/check_docs.py
python3 tools/release/check_runtime_resources.py
python3 tools/release/check_version_contract.py
python3 tools/release/check_sim_protobuf_runtime.py
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

`git archive` 不会带出原仓库的 `.git` 历史，但它也不会替代对导出内容的二次扫描。首次提交前应使用独立的敏感信息扫描器检查整个导出目录，并复核大文件列表。不要把研发仓库 remote、任务工件、本地 profile 或归档资料复制到新仓库。

打包检查只读取 Git 索引。因此在新目录运行前必须先 `git add --all` 建立待发布索引；未跟踪文件和
未暂存改动不会被检查器拿来替已经固定的发布快照背书。

## 4. 建立新的 Git 历史

只有导出目录通过全部门禁后，才创建首个提交。提交前先确认作者身份是真实维护者信息，不使用
`example.com`、占位身份或未确认的协作者尾注：

```bash
git -C "$RM26_PUBLIC_DIR" config user.name '<维护者姓名或团队公开身份>'
git -C "$RM26_PUBLIC_DIR" config user.email '<已确认的公开邮箱>'
git -C "$RM26_PUBLIC_DIR" commit -m "chore: 初始化公开仓库"
```

首次推送前再检查一次公开仓库历史。根提交应只包含维护者确认的作者信息和一条项目初始化说明：

```bash
git -C "$RM26_PUBLIC_DIR" log --format='%h %s' --all
git -C "$RM26_PUBLIC_DIR" log --format='%an <%ae>' --all | sort -u
```

如果出现占位作者、额外协作者尾注、无关任务描述或不能复核的完成声明，停止推送并重新生成
公开根提交，不回头改写研发仓库。

后续提交按一次改动只解决一类问题的方式组织，标题采用项目既有的简洁中文习惯，例如
`fix: 修复辅助图传分片恢复`、`docs: 补充模拟器联调说明`。提交信息只写实际变更和必要背景，
不记录无关过程或无从复核的完成声明。

## 5. 生成发行物

### 源码模式

只从公开仓库固定提交生成源码归档。Release 页面应明确这是源码版，需要用户自行安装依赖和构建；
不得上传内部仓库的 `.app`、wheel 或 `build/` 内容作为附带安装包。

归档完成后生成文件级 SPDX SBOM 和校验和：

```bash
SOURCE_DATE_EPOCH="$(git show -s --format=%ct HEAD)" \
python3 tools/release/generate_source_sbom.py \
  --root . \
  --output dist/source.spdx.json \
  --license '<已确认的 SPDX 标识>'

python3 tools/release/generate_checksums.py \
  dist/RM26CustomClient-<version>.tar.gz \
  dist/source.spdx.json \
  --output dist/SHA256SUMS
```

许可证未确认时不得用 `NOASSERTION` 的内部复核 SBOM 代替开源授权。首个源码归档暂不签名，
具体边界见[首个源码发行版签名策略](signing-policy.md)。

### 二进制模式

只为 `packaging_policy.json` 已声明的平台生成产物。每个平台都应从干净环境执行安装、在源码树外
启动并验证必要资源；不得用某台开发机上能运行替代安装验收。

所有模式都在最终归档完成后生成：

- SPDX 或 CycloneDX SBOM；
- 与实际随包组件对应的第三方许可证包；
- 覆盖每个公开文件的 `SHA256SUMS`；
- 策略要求的签名、公证或明确不签名说明。

归档内容发生任何变化后必须重新生成校验和和签名。

## 6. 发布后验证

- 从公开远端重新 clone，而不是复用导出目录。
- 按 README 的新贡献者路径完成一次全新构建和测试。
- 检查中英文 README、许可证、第三方声明、素材清单和安全联系方式。
- 重新下载所有发行物并复核校验和、签名、SBOM 和许可证包。
- 保存 CI 链接、发布提交、校验和和已知限制。
- 内部仓库继续保留完整研发历史；公开仓库只接收经过审查的后续提交。

首个公开仓库必须保留这条无历史镜像边界。内部仓库后续即使单独开展历史治理，也不改变首次公开
采用新仓库和新历史的要求。
