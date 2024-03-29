# тестовое задание

Исходное задение: 
На входе есть большой текстовый файл, где каждая строка имеет вид Number. String.

Например:

415. Apple

30432. Something something something

418. Apple

419. Cherry is the best

420. Banana is yellow

Обе части могут в пределах файла повторяться. Необходимо получить на выходе другой файл, где
все строки отсортированы. Критерий сортировки: сначала сравнивается часть String, если она
совпадает, тогда Number.

Т.е. в примере выше должно получиться

1. Apple

415. Apple

2. Banana is yellow

32. Cherry is the best

30432. Something something something

Требуется написать две программы:

1. Утилита для создания тестового файла заданного размера. Результатом работы должен быть
текстовый файл описанного выше вида. Должно быть какое-то количество строк с одинаковой
частью String.

2. Собственно сортировщик. Важный момент, файл может быть очень большой. Для тестирования
будет использоваться размер ~100Gb.

При оценке выполненного задания мы будем в первую очередь смотреть на результат
(корректность генерации/сортировки и время работы), во вторую на то, как кандидат пишет код.

Язык программирования: C++.

Как ориентир по времени - 10Гб файл генерируется около 2,5 минут, сортируется - 9 (кстати, это не самое быстрое решение). 

# комментарий к решению
в качестве алгоритма сортировки выбран "k-way merge sort", точка входа алгоритма: void sortBigFile(...).

1. сначала исходный файл размечается на части (chunks), примерно равного размера и примерно равного opts.chunkSize (по умолчанию 1 гб).

2. Затем, в функции asyncChunkSort парралельно запускаются асинхронные функции по сортировке каждой части и сохранению её на диск. При этом одновременно могут работать только 4 такие функции для экономии оперативной памяти (используется семафор). Попутно каждая часть еще раз размечается на части меньшего размера (micro-chunk-и), информация о micro-chunk-ах сохраняется в vector\<IterativeFile\>. 
(microChunkSize = chunkSize / chunksCount)

3. Далее в функции merge происходит финальная склейка частей в итоговый файл: Для каждого chunk-а последовательно грузятся его micro-chunk-и, и выполняется склейка частей с помощью очереди с приоритетом. Загрузка micro-chunk-ов и их разметка (сбор LineInfo) происходит асинхронно от самого алгоритма.

Все асинхронные действия выполнял с помощью синтаксиса async.

Все операции с файловой системой защищены мьютексом в классе ElementaryFileOperations (что-бы не было замедления при одновременном чтении-записи из разных потоков). Возможно для ssd-дисков это не проблема, но я не проверял.

Также, для удобной асинхронной записи в файл написан собственный наследник от  std::ostream и std::streambuf, который сбрасывает данные в отдельном потоке (AsyncOstream). Он тоже в итоге обращается к ElementaryFileOperations::write

Для генерации файлов с одинаковыми строками я решил сделать просто: заранее сгенерировать кэш с строками и числами, а затем брать из этого кэша случайные элементы. Таким образом, если выбрать определенные размеры кэша, можно обеспечить 100% вероятность того, что в итоговом файле будут повторяющиеся строки.

Для парсинга аргументов командной строки я использовал чужой код ( https://www.codeproject.com/Tips/5261900/Cplusplus-Lightweight-Parsing-Command-Line-Argumen ).

Для удобного вывода на консоль использовал header-only библиотеку https://github.com/gabime/spdlog (исходники закоммитил вместе с проектом для удобства).

При тестировании на моей системе график загрузки диска всегда показывал 100%, график загрузки CPU низкий. 
для HDD:
Время генерации 10-гб файла: 2 минуты.
Время сортировки этого файла: 9:30 минут. 
для SSD:
Время генерации 10-гб файла: 40 секунд.
Время сортировки этого файла: 3:30 минут. 


TODO-list:
1. Случай маленьких файлов отдельно не обрабатываю, в реальном продакшн-коде конечно не нужно перезаписывать маленькие файлы 2 раза на диск, достаточно один раз прогнать сортировку
2. Кое-где в коде функторов бросаются и не ловятся исключения, в целом не проверял работу на ошибочных файлах. Так-же отдельно стоит рассматривать файлы с очень длинными строками.
3. Уже после того как сделал, понял что этот код смотрелся бы лучше если воспользоваться системой задач с зависимостями, например через boost::asio.
4. Возможно можно увеличить скорость работы с диском, если порыть более низкоуровневые интерфейсы чем fstream и fopen().

Параметры запуска генератора файлов (в фигурных скобках значения по умолчанию):

filesize{10737418240} - размер генерируемого файла (реальный размер может быть немного больше)

filepath{"file.txt"} - путь к файлу

stringsCount{5000} - число строк в кэше

numsCount{10000} - число чисел в кэше

maxStrLen{50} - максимальная длина строки

maxNum{100000} - максимальное число 

fullRandom{false} - если включен, то каждая строка и каждое число будет генерироваться отдельно, без создания кэша.

параметры запуска сортировщика:

chunkSize{1073741824} - размер chunk-ов, на которые делится исходный файл. 

inputFile{"file.txt"} - входной файл

outputFile{"out.txt"} - выходной файл

cacheDir{""}; - директория для временного хранения отсортированных chunk-ов

пример запуска для примерно 100-мегабайтного файла, который разобъется примерно на 10 частей:

./filegenereator.exe --filesize 100000000 --filepath input100000000.txt --stringsCount 1000

./sorter.exe --inputFile input100000000.txt --outputFile output100000000.txt --chunkSize 10000000


