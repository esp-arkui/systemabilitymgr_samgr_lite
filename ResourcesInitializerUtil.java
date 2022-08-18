package com.huawei.ci.portal.provider.utils;

import com.huawei.ci.portal.provider.enums.assistanthelper.AssistantConstants;
import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.io.*;
import java.util.*;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.stream.Collectors;

/**
 * @Description: 资源文件初始化处理类
 * @ClassName: com.huawei.ci.portal.provider.utils
 * @Author: hwx1123794/hexiangyun
 * @DateTime: 2022/7/21  10:04
 * @Params:
 **/
@Data
@AllArgsConstructor
@Component
public class ResourcesInitializerUtil {

    private static final Logger logger = LoggerFactory.getLogger(ResourcesInitializerUtil.class);

    private static StringBuilder replaceAll;

    private static String encoding = AssistantConstants.ENCODING;

    private static String replaceStr = AssistantConstants.BE_REPLACE_STR;

    private static int replaceSize = AssistantConstants.BE_REPLACE_SIZE;

    private static String fileName = AssistantConstants.FILE_PATH;

    private static HashSet<String> strings = new HashSet<>(100);

    private static Set<String> allSensitiveWordSet = new HashSet<>();

    private static Map afterDealingSensitiveWords = new HashMap<>();


    /**
     * 文件
     *
     * @param fileName 词库文件名(含后缀)
     */
    public ResourcesInitializerUtil(String fileName) {
        this.fileName = fileName;
    }


    /**
     * 初始化敏感词库
     *
     * @return
     */
    protected Map InitializationWorkUtil() {
        replaceAll = new StringBuilder(replaceSize);
        for (int x = 0; x < replaceSize; x++) {
            replaceAll.append(replaceStr);
        }
        InputStreamReader read = null;
        BufferedReader bufferedReader = null;
        ReentrantReadWriteLock reentrantReadWriteLock = new ReentrantReadWriteLock();
        // 读锁加锁
        reentrantReadWriteLock.readLock().lock();
        try {
            logger.info("Init sensitive-words configuration start ");
            read = new InputStreamReader(Objects.requireNonNull(ResourcesInitializerUtil.class.getClassLoader()
                    .getResourceAsStream(fileName)), encoding);
            // 缓冲区大小  手动设置为80K
            bufferedReader = new BufferedReader(read, 81920);
            allSensitiveWordSet = bufferedReader.lines().collect(Collectors.toSet());
            // 构造属性
            afterDealingSensitiveWords = addSensitiveWords2HashConstruct(allSensitiveWordSet);
            logger.info("Init sensitive-words configuration success ");

        } catch (IOException e) {
            logger.error("read file failed");
        } finally {
            try {
                if (null != bufferedReader)
                    // 缓冲流关闭
                    bufferedReader.close();
            } catch (IOException e) {
                logger.error("shutdown Buffered streamline failed, caused by: " + e);
            }
            try {
                if (null != read)
                    // IO流关闭
                    read.close();
            } catch (IOException e) {
                logger.error("shutdown IO streamline failed , caused by: " + e);
            }
            // 释放
            reentrantReadWriteLock.readLock().unlock();
        }
        return afterDealingSensitiveWords;
    }


    /**
     * DFA算法构造敏感词树形结构 (凡是相同字符开头的都在同一个HashMap + 树形结构 )
     *
     * @param sensitiveWords 从文件中读取的敏感词集合
     * @return
     */
    private HashMap addSensitiveWords2HashConstruct(Set<String> sensitiveWords) {
        // 关键字 整理成 Hash->树形 结构。
        HashMap sensitiveWordsMap = new HashMap(sensitiveWords.size());
        String currentWord = null;
        Map childMap = null;
        Map<String, String> newWordMap = new HashMap<>();
        // 处理敏感词
        Iterator<String> iterator = sensitiveWords.iterator();
        while (iterator.hasNext()) {
            currentWord = iterator.next();
            childMap = sensitiveWordsMap;
            // 关键字构造树形结构
            for (int i = 0; i < currentWord.length(); i++) {
                char key = currentWord.charAt(i);
                Object wordMap = childMap.get(key);
                if (wordMap != null) {
                    childMap = (Map) wordMap;
                } else {
                    newWordMap = new HashMap<>();
                    // 添加标记位
                    newWordMap.put("isEnd", "0");
                    childMap.put(key, newWordMap);
                    childMap = newWordMap;
                }
                // 最后一位
                if (i == currentWord.length() - 1) {
                    childMap.put("isEnd", "1");
                }
            }
        }
        return sensitiveWordsMap;
    }
}
