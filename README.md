# QSettings JSON Formatter
Json Formatter for `QSettings`

[Qt](https://www.qt.io/) provides `QSettings`, a class for read and write your applications settings. Originally `QSettings` only provides reading and writing [INI-Files](https://en.wikipedia.org/wiki/INI_file), but `QSettings` provides an interface to declare your own read and write function set as part of a `Formatter` definition. We use these for reading and writing settings in [JSON](https://en.wikipedia.org/wiki/JSON) file format.

## Why QSettings?

Using `QSettings` instead of your own solution is a good idea because `QSettings` is already [reentrant](https://doc.qt.io/qt-5/threads-reentrancy.html) so using the same settings data source by different processes is easy because the access is managed by `QSettings` in the background. 

## Usage

The usage is shown in the main.cpp file, just define a new `QSettings` object using the new Formatter
```
QSettings settings("config.json", JsonFormat);
```
do some error handling as shown and access the values:
```
QStringList allKeys = settings.allKeys();
for (const QString &key : qAsConst(allKeys)){
	qDebug().noquote() << key << ": " << settings.value(key);
}
```
You can add new values using the hierarchy delimit character ('/')
```
settings.setValue("test1/test1.3", QString("v_test1.3"));
 ```

If the `QSettings` object was changed, it will be stored automatically at the end of your program. You can force the memory-file synchronization (for instance for interprocess communication) using 
```
settings.sync();
```

## Project Description

This project provides a set of functions to read and write data in UTF-8 encoded, nested JSON format. You can find other projects with the same focus, the mostly flatten down the json structure during the read-write process.

Currently all settings stored in a `QSettings` object are stored in a flat `QMap` using key-value pairs. Each key is a `QString`, each value is a [`QVariant`](https://doc.qt.io/qt-5/qvariant.html) of the expected type. Nesting data is not supported in `QSettings` by default (except one group per pair as in the standard [INI File format](https://en.wikipedia.org/wiki/INI_file)) and special data types like *array* or *object* are also not supported as part of the key-value-pair. 

There is a conversion function to store JSON data in a so called `QVariantMap` (basically a `QMap`), but this function will flatten down the nested structure to simple key-value pairs in the map. All nesting information will be lost. 

### The idea

#### Reading JSON

Reading a given JSON structure simply is done using the [Qt JSON](https://doc.qt.io/qt-5/json.html) parser classes ([`QJsonDocument`](https://doc.qt.io/qt-5/qjsondocument.html), [`QJsonObject`](https://doc.qt.io/qt-5/qjsonobject.html), [`QJsonArray`](https://doc.qt.io/qt-5/qjsonarray.html), [`QJsonValue`](https://doc.qt.io/qt-5/qjsonvalue.html), and so on). The Qt JSON parser walks deep-first through the elements of the JSON data tree and we simply had to ask for the data type of each JSON element and acting with the data according to the data type. Each element consists of a *name*-value pair (*name*-value pairs are comparable with the *key*-value pairs of a map). The data types *object* and *array* are nested elements, as mentioned these will be handled by special parse functions. 

Our function `parseJsonValue` is responsible for this, it determines the data type of the current JSON element and calls the appropriate parse function: Simple JSON data types will be inserted directly into the map, *object*s and *array*s will be handled by their parsing functions: 

* The `parseJsonObject`function is responsible for JSON *object*s. All sub elements will be inserted into the map. The name of the JSON element will be used as [part of the key](#Hierarchical-keys-in-a-flat-map) in the map.

* `parseJsonArray` is responsible for JSON *array*s.  All sub elements will be inserted into the map, too. JSON Array item have no explicit name, I used a counter for the array items as implicit name to preserve the order of the items. The name will be marked with a hash sign at the beginning. This generated name will be used as [part of the key](#Hierarchical-keys-in-a-flat-map) in the map.

##### Hierarchical keys in a flat map

To be able to determine the hierarchy of the JSON elements, the name of the current and the names of the parent and grandparent levels are simply concatenated with a slash ('/') to a *compound key*. That way, a nested JSON file like 

```json
{
	"test1" : {
		"testA" : "v_testA",
		"test1.2" : "v_test1.1",
		"emptyarraytest": [],
		"subobject": {
			"yes": true,
			"no" : "non"
		}
	},
	"myArray" : [
		"asd",
		45,
		{},
		null,
		"long\r\nmultiline \r\ntext",
		{
			"yes": true,
			"no" : "pas du tout"
		}
	]
}
```

will be stored in the map as 

| key                         | value                                   |
| --------------------------- | --------------------------------------- |
| myArray/#0000               | asd (string)                            |
| myArray/#0001               | 45 (double)                             |
| myArray/#0002               | null (null)                             |
| myArray/#0003               | long<br />multiline <br />text (string) |
| myArray/#0004/no            | pas du tout (string)                    |
| myArray/#0004/yes           | true (bool)                             |
| test1/test1.2               | v_test1.1 (string)                      |
| test1/test1.2/subobject/no  | non (string)                            |
| test1/test1.2/subobject/yes | true (bool)                             |
| test1/testA                 | v_testA (string)                        |

As you can see, there are some differences in both data representations: 
* The elements are now ordered alphabetical by key, this is a feature of the `QMap`. 
* The JSON elements `"emptyarraytest": [] ` and `{}` were omitted: Even these elements are correct JSON elements they do not contain any *value* usable in a key-value map. 

So keep in mind, the implicit name of an array element could be different from that of the original data source. And also keep in mind, JSON only provides a few simple data types by design, so some type conversion could take place you should have an eye on. For instance JSON only defines the data type number, the Qt JSON tool store these values as doubles, so the value `54` (probably an integer) is not stored as a value of data type `integer`.  More on these compatibility issues you will find on [Wikipedia-JSON](https://en.wikipedia.org/wiki/JSON), the official website [json.org](https://www.json.org), and in the final draft of the [ECMA JSON standard description](https://www.ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf).

Actually, the hierarchy divider character ('/') is not escaped during the reading process, so if you use the '/' in one of your JSON keys, it will by injected as a new level of hierarchy. The same is true for other critical characters, they are actually not handled or escaped.

#### Writing JSON

##### What wont work with Qt JSON

The first idea was to simply walk over the all pairs in the map and store the value using recursive function calls at the right place in an evolving JSON tree. This sounds easy but the [QJSON tool set of Qt](https://doc.qt.io/qt-5/json.html) does not allow to change elements once added to the JSON tree: You actually can't get a reference to an already added JSON element, so adding new sub elements after having added a JSON element to the tree is not possible. 

One solution will be the use an other JSON parser library, we like to use the given one.

#####  Our write solution

Because it is impossible to add a new sub elements to an element already added to a JSON tree (for instance a new item to a [`QJsonArray`](https://doc.qt.io/qt-5/qjsonarray.html)), all sub elements must be added to the parent element before it is added as a child to it's parent element in the JSON tree. For instance all array item elements, like [`QJsonArray`](https://doc.qt.io/qt-5/qjsonarray.html)s, [`QJsonObject`](https://doc.qt.io/qt-5/qjsonobject.html)s or [`QJsonValue`](https://doc.qt.io/qt-5/qjsonvalue.html)s, have to be added to the parent array element before added this parent array element to it's parent. Of course, this is true recursively. 

This is possible by walking on both structures at the same time: the map on the one hand and the evolving JSON tree on the other. We have to start with a map sorted by key (luckily `QMap`s are always sorted by key). For the first key-value pair of the map

1. we split the key (our *compound key*) by the hierarchy delimit character ('/'). We call each part a *section*, all parts together *list of sections* or simply *sections*.
2. For each section, one after the other, we have to determine the section type (see `restoreJsonValue`). For that, we are using a section index called `sectionlevel` to remember the section currently in focus. 
   1. If the current section is the last section in the section list, it will be a [`QJsonValue`](https://doc.qt.io/qt-5/qjsonvalue.html) and we return the wrapped value back to the calling function.
   2. If the current section isn't the last section (there are proceeding sections) and the next section (`sectionlevel` + 1) starts with our array index marker character ('#') and the section name is convertible into an integer value, the current section is of type [`QJsonArray`](https://doc.qt.io/qt-5/qjsonarray.html).
   3. Otherwise, the current section is of type [`QJsonObject`](https://doc.qt.io/qt-5/qjsonobject.html).
3. That way we create the JSON strcture for the first map item by recursively calling for each section the appropriate parsing function. Each parsing function stores the full path of the current section, the level of the section and the JSON element (without inserting the element into the parent element). Determined sub elements are added to this JSON element in a loop.
4. Finally (if there are no more sub elements) we return the JSON element to the parent JSON element parsing function (see step 2.1).


So far for the first map item. Instead of finally walking all the way back to the root element of the tree and start the process again with the next key we use another procedure: When we reached the point were we stored the value into a JSON element, we directly read the next key-value pair from the map. Because the `QMap` is sorted, we can be sure, all siblings are grouped together. We split the key as described in step 1 and store the sections. Then we go back to the parent, store the simple value and

7. in the parent element parsing function we check if the new key and the full key of the current section do match: 
   * if they do not match, we finish the element construction, step back to the parent element parsing function, store the finished element as value and do this check again.
   * If they do match, we process the new key starting step 2 in.
 
