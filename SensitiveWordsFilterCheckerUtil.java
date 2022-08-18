package com.huawei.ci.portal.provider.utils;

import com.huawei.ci.portal.provider.enums.assistanthelper.AssistantConstants;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.util.*;

/**
 * @Description: 敏感词过滤工具，过滤不雅词汇
 * @ClassName: com.huawei.ci.portal.provider.utils
 * @Author: hwx1123794/hexiangyun
 * @DateTime: 2022/7/21  15:54
 * @Params:
 **/
@Component
public class SensitiveWordsFilterCheckerUtil {

    private static final Logger logger = LoggerFactory.getLogger(SensitiveWordsFilterCheckerUtil.class);

    private static final Map sensitiveWordsMap;

    // 最小匹配
    private static final Integer minMatchType = 1;

    // 完全匹配
    private static final Integer maxMatchType = 2;

    static {
        sensitiveWordsMap = new ResourcesInitializerUtil().InitializationWorkUtil();
    }


    /**
     * 校验是否包含敏感词
     *
     * @param txt       待判断文本
     * @param start     起始位置
     * @param matchType 匹配类型： 1 最小匹配原则；2 最大匹配原则
     * @return 大于0表示包含敏感词且表示敏感词匹配长度，否则不包含
     */
    private static Integer checkIfExistSensitiveWords(String txt, Integer start, Integer matchType) {
        Boolean flag = false;
        char word;
        // 匹配标记位
        Integer matchFlag = 0;
        Map childMap = sensitiveWordsMap;
        for (int i = start; i < txt.length(); i++) {
            word = txt.charAt(i);
            childMap = (Map) childMap.get(word);
            if (childMap == null) {
                break;
            } else {
                // 匹配标记位+1
                matchFlag++;
                //  isEnd标记位 = 1时，匹配到了末尾
                if ("1".equals(childMap.get("isEnd"))) {
                    flag = true;
                    if (minMatchType.equals(matchType)) {
                        break;
                    }
                }
            }
        }
        if (matchFlag < 2 || !flag) {
            // 匹配长度需大于2才为词，并且敏感词已结束
            matchFlag = 0;
        }
        return matchFlag;
    }

    /**
     * 获取所有敏感词
     *
     * @param txt           待判断文本
     * @param matchType     匹配类型： 1 最小匹配原则；2 最大匹配原则
     * @return
     */
    public static Set<String> getSensitiveWords(String txt, Integer matchType) {
        Set<String> sensitiveWords = new HashSet<>();
        for (int i = 0; i < txt.length(); i++) {
            // 判断敏感词所在文本内容中的 起始点
            Integer length = checkIfExistSensitiveWords(txt, i, matchType);
            if (length > 0) {
                sensitiveWords.add(txt.substring(i, i + length));
                // 循环i会+1，所以需-1
                i = i + length - 1;
            }
        }
        return sensitiveWords;
    }

    /**
     * 替换敏感词
     *
     * @param txtOfInput 文本
     * @param matchType  匹配类型： 1 最小匹配原则；2 最大匹配原则
     * @param replaceStr 替换字符
     * @return           处理后的文本
     */
    public String replaceSensitiveWords(String txtOfInput, Integer matchType, String replaceStr) {
        if (txtOfInput == null || "".equals(txtOfInput)) {
            return txtOfInput;
        }
        Set<String> sensitiveWords = getSensitiveWords(txtOfInput, matchType);
        Iterator<String> iterator = sensitiveWords.iterator();
        String replaceString = "";
        while (iterator.hasNext()) {
            String sWord = iterator.next();
            // 替换敏感词内容
            replaceString = getReplaceString(replaceStr, sWord.length());
            txtOfInput = txtOfInput.replaceAll(sWord, replaceString);
        }
        return txtOfInput;
    }

    /**
     * 获取需要替换的文本
     *
     * @param length     要替换的文本长度
     * @param replaceStr 将要替换的文本符号
     */
    private static String getReplaceString(String replaceStr, Integer length) {
        if (replaceStr == null) {
            // 使用 * 替换敏感词。 在 AssistantConstants中配置
            replaceStr = AssistantConstants.BE_REPLACE_STR;
        }
        StringBuffer replaceString = new StringBuffer();
        for (int i = 0; i < length; i++) {
            replaceString.append(replaceStr);
        }
        return replaceString.toString();
    }

}

